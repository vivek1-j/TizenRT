/****************************************************************************
 * arch/arm/src/qemu/qemu_serial.c
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <tinyara/irq.h>
#include <tinyara/arch.h>
#include <tinyara/serial/serial.h>
#include <tinyara/spinlock.h>
#include <tinyara/semaphore.h>

#ifdef CONFIG_SERIAL_TERMIOS
#  include <termios.h>
#endif

#include <arch/serial.h>
#include <arch/board/board.h>

#include "chip.h"
#include "up_arch.h"
#include "up_internal.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* PL011 UART Register Offsets */

#define PL011_DR        0x000   /* Data Register */
#define PL011_RSR_ECR   0x004   /* Receive Status/Error Clear Register */
#define PL011_FR        0x018   /* Flag Register */
#define PL011_IBRD      0x024   /* Integer Baud Rate Register */
#define PL011_FBRD      0x028   /* Fractional Baud Rate Register */
#define PL011_LCR_H     0x02C   /* Line Control Register */
#define PL011_CR        0x030   /* Control Register */
#define PL011_IFLS      0x034   /* Interrupt FIFO Level Select Register */
#define PL011_IMSC      0x038   /* Interrupt Mask Set/Clear Register */
#define PL011_RIS       0x03C   /* Raw Interrupt Status Register */
#define PL011_MIS       0x040   /* Masked Interrupt Status Register */
#define PL011_ICR       0x044   /* Interrupt Clear Register */
#define PL011_DMACR     0x048   /* DMA Control Register */

/* PL011 Flag Register Bits */

#define PL011_FR_CTS    (1 << 0)
#define PL011_FR_DSR    (1 << 1)
#define PL011_FR_DCD    (1 << 2)
#define PL011_FR_BUSY   (1 << 3)
#define PL011_FR_RXFE   (1 << 4)  /* Receive FIFO Empty */
#define PL011_FR_TXFF   (1 << 5)  /* Transmit FIFO Full */
#define PL011_FR_RXFF   (1 << 6)  /* Receive FIFO Full */
#define PL011_FR_TXFE   (1 << 7)  /* Transmit FIFO Empty */

/* PL011 Line Control Register Bits */

#define PL011_LCR_H_BRK     (1 << 0)
#define PL011_LCR_H_PEN     (1 << 1)
#define PL011_LCR_H_EPS     (1 << 2)
#define PL011_LCR_H_STP2    (1 << 3)
#define PL011_LCR_H_FEN     (1 << 4)
#define PL011_LCR_H_WLEN_SHIFT  5
#define PL011_LCR_H_WLEN8   (0x3 << PL011_LCR_H_WLEN_SHIFT)

/* PL011 Control Register Bits */

#define PL011_CR_UARTEN     (1 << 0)
#define PL011_CR_SIREN      (1 << 1)
#define PL011_CR_SIRLP      (1 << 2)
#define PL011_CR_LBE        (1 << 7)
#define PL011_CR_TXE        (1 << 8)
#define PL011_CR_RXE        (1 << 9)
#define PL011_CR_DTR        (1 << 10)
#define PL011_CR_RTS        (1 << 11)
#define PL011_CR_RTSEn      (1 << 14)
#define PL011_CR_CTSEn      (1 << 15)

/* PL011 Interrupt Mask Bits */

#define PL011_IMSC_RIMIM    (1 << 0)
#define PL011_IMSC_CTSMIM   (1 << 1)
#define PL011_IMSC_DCDMIM   (1 << 2)
#define PL011_IMSC_DSRMIM   (1 << 3)
#define PL011_IMSC_RXIM     (1 << 4)
#define PL011_IMSC_TXIM     (1 << 5)
#define PL011_IMSC_RTIM     (1 << 6)
#define PL011_IMSC_FEIM     (1 << 7)
#define PL011_IMSC_PEIM     (1 << 8)
#define PL011_IMSC_BEIM     (1 << 9)
#define PL011_IMSC_OEIM     (1 << 10)

#define PL011_IMSC_MASK_ALL 0x7FF

/* UART base address and IRQ */

#define PL011_BASE          (QEMU_UART1_BASE)
#define PL011_IRQ           (33)  /* UART1 IRQ */

/* Register access macros */

#define pl011_putreg(offset, value) \
    (*(volatile uint32_t *)(PL011_BASE + (offset)) = (value))
#define pl011_getreg(offset) \
    (*(volatile uint32_t *)(PL011_BASE + (offset)))

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct qemu_uart_dev_s {
    bool rxint_enable;
    bool txint_enable;
    spinlock_t lock;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int qemu_uart_setup(FAR struct uart_dev_s *dev);
static void qemu_uart_shutdown(FAR struct uart_dev_s *dev);
static int qemu_uart_attach(FAR struct uart_dev_s *dev);
static void qemu_uart_detach(FAR struct uart_dev_s *dev);
static int qemu_uart_ioctl(FAR struct uart_dev_s *dev, int cmd, unsigned long arg);
static int qemu_uart_receive(FAR struct uart_dev_s *dev, FAR unsigned int *status);
static void qemu_uart_rxint(FAR struct uart_dev_s *dev, bool enable);
static bool qemu_uart_rxavailable(FAR struct uart_dev_s *dev);
static void qemu_uart_send(FAR struct uart_dev_s *dev, int ch);
static void qemu_uart_txint(FAR struct uart_dev_s *dev, bool enable);
static bool qemu_uart_txready(FAR struct uart_dev_s *dev);
static bool qemu_uart_txempty(FAR struct uart_dev_s *dev);

static int qemu_uart_interrupt(int irq, void *context, void *arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* I/O buffers */

static char g_qemu_rxbuffer[CONFIG_UART1_RXBUFSIZE];
static char g_qemu_txbuffer[CONFIG_UART1_TXBUFSIZE];

/* Device private data */

static struct qemu_uart_dev_s g_uart_priv = {
    .rxint_enable = false,
    .txint_enable = false,
    .lock = SP_UNLOCKED,
};

/* uart_dev_t instance for console */

static uart_dev_t g_uart_port = {
    .isconsole = true,
    .recv = {
        .size = CONFIG_UART1_RXBUFSIZE,
        .buffer = g_qemu_rxbuffer,
    },
    .xmit = {
        .size = CONFIG_UART1_TXBUFSIZE,
        .buffer = g_qemu_txbuffer,
    },
    .ops = NULL,  /* Will be set below */
    .priv = &g_uart_priv,
};

/* UART operations structure */

static const struct uart_ops_s g_uart_ops = {
    .setup       = qemu_uart_setup,
    .shutdown    = qemu_uart_shutdown,
    .attach      = qemu_uart_attach,
    .detach      = qemu_uart_detach,
    .ioctl       = qemu_uart_ioctl,
    .receive     = qemu_uart_receive,
    .rxint       = qemu_uart_rxint,
    .rxavailable = qemu_uart_rxavailable,
#ifdef CONFIG_SERIAL_IFLOWCONTROL
    .rxflowcontrol = NULL,
#endif
    .send        = qemu_uart_send,
    .txint       = qemu_uart_txint,
    .txready     = qemu_uart_txready,
    .txempty     = qemu_uart_txempty,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: qemu_uart_disable
 ****************************************************************************/

static void qemu_uart_disable(void)
{
    uint32_t cr = pl011_getreg(PL011_CR);
    cr &= ~PL011_CR_UARTEN;
    pl011_putreg(PL011_CR, cr);
}

/****************************************************************************
 * Name: qemu_uart_enable
 ****************************************************************************/

static void qemu_uart_enable(void)
{
    uint32_t cr = pl011_getreg(PL011_CR);
    cr |= PL011_CR_UARTEN;
    pl011_putreg(PL011_CR, cr);
}

/****************************************************************************
 * Name: qemu_uart_setup
 ****************************************************************************/

static int qemu_uart_setup(FAR struct uart_dev_s *dev)
{
    uint32_t regval;
    irqstate_t flags;

    flags = enter_critical_section();

    /* Disable the UART before configuration */
    qemu_uart_disable();

    /* CRITICAL: Mask all interrupts and clear pending */
    pl011_putreg(PL011_IMSC, 0);           /* Mask ALL interrupts */
    pl011_putreg(PL011_ICR, PL011_IMSC_MASK_ALL);  /* Clear all pending */

    /* Set baud rate (assuming 24MHz UART clock, 115200 baud) */
    /* IBRD = 24,000,000 / (16 * 115200) = 13.02 -> 13 */
    pl011_putreg(PL011_IBRD, 13);
    /* FBRD = 0.02 * 64 = 1.28 -> 1 */
    pl011_putreg(PL011_FBRD, 1);

    /* 8-bit, no parity, 1 stop bit, enable FIFOs */
    regval = PL011_LCR_H_WLEN8 | PL011_LCR_H_FEN;
    pl011_putreg(PL011_LCR_H, regval);

    /* Clear DMA control */
    pl011_putreg(PL011_DMACR, 0);

    /* Clear flow control bits */
    regval = pl011_getreg(PL011_CR);
    regval &= ~(PL011_CR_RTSEn | PL011_CR_CTSEn | PL011_CR_SIREN);
    pl011_putreg(PL011_CR, regval);

    /* Enable RX and TX (but not UART yet) */
    regval |= PL011_CR_RXE | PL011_CR_TXE;
    pl011_putreg(PL011_CR, regval);

    leave_critical_section(flags);

    /* Enable the UART */
    qemu_uart_enable();

    return OK;
}

/****************************************************************************
 * Name: qemu_uart_shutdown
 ****************************************************************************/

static void qemu_uart_shutdown(FAR struct uart_dev_s *dev)
{
    qemu_uart_disable();
}

/****************************************************************************
 * Name: qemu_uart_attach
 ****************************************************************************/

static int qemu_uart_attach(FAR struct uart_dev_s *dev)
{
    int ret;

    /* Attach the IRQ handler */
    ret = irq_attach(PL011_IRQ, qemu_uart_interrupt, dev);
    if (ret == OK) {
        /* Enable the IRQ */
        up_enable_irq(PL011_IRQ);
    }

    return ret;
}

/****************************************************************************
 * Name: qemu_uart_detach
 ****************************************************************************/

static void qemu_uart_detach(FAR struct uart_dev_s *dev)
{
    /* Disable the IRQ */
    up_disable_irq(PL011_IRQ);

    /* Detach the IRQ handler */
    irq_detach(PL011_IRQ);
}

/****************************************************************************
 * Name: qemu_uart_ioctl
 ****************************************************************************/

static int qemu_uart_ioctl(FAR struct uart_dev_s *dev, int cmd, unsigned long arg)
{
    int ret = OK;

    switch (cmd) {
    case TIOCSBRK:  /* BSD compatibility: Turn break on */
    case TIOCCBRK:  /* BSD compatibility: Turn break off */
        break;

#ifdef CONFIG_SERIAL_TERMIOS
    case TCGETS: {
        FAR struct termios *termiosp = (struct termios *)arg;
        if (!termiosp) {
            return -EINVAL;
        }
        /* Return default settings */
        termiosp->c_cflag = B115200 | CS8 | CREAD | CLOCAL;
        termiosp->c_iflag = 0;
        termiosp->c_oflag = OPOST | ONLCR;
        termiosp->c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK;
    }
    break;

    case TCSETS: {
        FAR struct termios *termiosp = (struct termios *)arg;
        if (!termiosp) {
            return -EINVAL;
        }
        /* Accept settings (we don't actually change hardware) */
    }
    break;
#endif

    default:
        ret = -ENOTTY;
        break;
    }

    return ret;
}

/****************************************************************************
 * Name: qemu_uart_receive
 ****************************************************************************/

static int qemu_uart_receive(FAR struct uart_dev_s *dev, FAR unsigned int *status)
{
    uint32_t dr;

    /* Read the data register (includes error status in upper bits) */
    dr = pl011_getreg(PL011_DR);
    *status = dr & 0xF00;  /* Error bits */

    return dr & 0xFF;  /* Return character */
}

/****************************************************************************
 * Name: qemu_uart_rxint
 ****************************************************************************/

static void qemu_uart_rxint(FAR struct uart_dev_s *dev, bool enable)
{
    struct qemu_uart_dev_s *priv = (struct qemu_uart_dev_s *)dev->priv;

    if (enable) {
        /* Enable RX and RX timeout interrupts */
        pl011_putreg(PL011_IMSC, pl011_getreg(PL011_IMSC) | PL011_IMSC_RXIM | PL011_IMSC_RTIM);
    } else {
        /* Disable RX and RX timeout interrupts */
        pl011_putreg(PL011_IMSC, pl011_getreg(PL011_IMSC) & ~(PL011_IMSC_RXIM | PL011_IMSC_RTIM));
    }
    priv->rxint_enable = enable;
}

/****************************************************************************
 * Name: qemu_uart_rxavailable
 ****************************************************************************/

static bool qemu_uart_rxavailable(FAR struct uart_dev_s *dev)
{
    /* Return true if RX FIFO is not empty */
    return !(pl011_getreg(PL011_FR) & PL011_FR_RXFE);
}

/****************************************************************************
 * Name: qemu_uart_send
 ****************************************************************************/

static void qemu_uart_send(FAR struct uart_dev_s *dev, int ch)
{
    /* Wait for TX FIFO to have space */
    while (pl011_getreg(PL011_FR) & PL011_FR_TXFF);

    /* Write character to data register */
    pl011_putreg(PL011_DR, ch);
}

/****************************************************************************
 * Name: qemu_uart_txint
 ****************************************************************************/

static void qemu_uart_txint(FAR struct uart_dev_s *dev, bool enable)
{
    struct qemu_uart_dev_s *priv = (struct qemu_uart_dev_s *)dev->priv;

    if (enable) {
        pl011_putreg(PL011_IMSC, pl011_getreg(PL011_IMSC) | PL011_IMSC_TXIM);
    } else {
        pl011_putreg(PL011_IMSC, pl011_getreg(PL011_IMSC) & ~PL011_IMSC_TXIM);
    }
    priv->txint_enable = enable;
    /* Note: uart_xmitchars() is called from the ISR, not here */
}

/****************************************************************************
 * Name: qemu_uart_txready
 ****************************************************************************/

static bool qemu_uart_txready(FAR struct uart_dev_s *dev)
{
    /* Return true if TX FIFO is not full */
    return !(pl011_getreg(PL011_FR) & PL011_FR_TXFF);
}

/****************************************************************************
 * Name: qemu_uart_txempty
 ****************************************************************************/

static bool qemu_uart_txempty(FAR struct uart_dev_s *dev)
{
    /* Return true if TX FIFO is empty */
    return pl011_getreg(PL011_FR) & PL011_FR_TXFE;
}

/****************************************************************************
 * Name: qemu_uart_interrupt
 ****************************************************************************/

static int qemu_uart_interrupt(int irq, void *context, void *arg)
{
    FAR struct uart_dev_s *dev = (FAR struct uart_dev_s *)arg;
    uint32_t mis;
    uint32_t icr = 0;

    /* Get masked interrupt status */
    mis = pl011_getreg(PL011_MIS);

    /* Check for RX interrupt */
    if (mis & (PL011_IMSC_RXIM | PL011_IMSC_RTIM)) {
        /* Pass received characters to the upper half */
        uart_recvchars(dev);
        /* Mark RX and RT interrupts for clearing */
        icr |= PL011_IMSC_RXIM | PL011_IMSC_RTIM;
    }

    /* Check for TX interrupt */
    if (mis & PL011_IMSC_TXIM) {
        /* Process TX data */
        uart_xmitchars(dev);
        /* Mark TX interrupt for clearing */
        icr |= PL011_IMSC_TXIM;
    }

    /* Clear the interrupts - write specific bits to ICR */
    pl011_putreg(PL011_ICR, icr);

    return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_earlyserialinit
 ****************************************************************************/

void up_earlyserialinit(void)
{
    uint32_t regval;

    /* Disable the UART before configuration */
    pl011_putreg(PL011_CR, 0);

    /* CRITICAL: Mask all interrupts and clear pending */
    pl011_putreg(PL011_IMSC, 0);
    pl011_putreg(PL011_ICR, PL011_IMSC_MASK_ALL);

    /* Set baud rate (assuming 24MHz UART clock, 115200 baud) */
    pl011_putreg(PL011_IBRD, 13);
    pl011_putreg(PL011_FBRD, 1);

    /* 8-bit, no parity, 1 stop bit, enable FIFOs */
    regval = PL011_LCR_H_WLEN8 | PL011_LCR_H_FEN;
    pl011_putreg(PL011_LCR_H, regval);

    /* Enable the UART, TX and RX */
    pl011_putreg(PL011_CR, PL011_CR_UARTEN | PL011_CR_TXE | PL011_CR_RXE);
}

/****************************************************************************
 * Name: up_serialinit
 ****************************************************************************/

void up_serialinit(void)
{
    /* Set the ops pointer */
    g_uart_port.ops = &g_uart_ops;

    /* Register the console device using the standard serial framework */
    uart_register("/dev/console", &g_uart_port);
}

/****************************************************************************
 * Name: up_putc
 ****************************************************************************/

int up_putc(int ch)
{
    /* Wait for TX FIFO to be not full */
    while (pl011_getreg(PL011_FR) & PL011_FR_TXFF);

    /* Send character, handling \n -> \r\n translation */
    if (ch == '\n') {
        while (pl011_getreg(PL011_FR) & PL011_FR_TXFF);
        pl011_putreg(PL011_DR, '\r');
    }

    pl011_putreg(PL011_DR, ch);

    /* Wait for TX FIFO to empty */
    while (!(pl011_getreg(PL011_FR) & PL011_FR_TXFE));

    return ch;
}

/****************************************************************************
 * Name: up_lowputc
 ****************************************************************************/

void up_lowputc(char ch)
{
    up_putc(ch);
}
