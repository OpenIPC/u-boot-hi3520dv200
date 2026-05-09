#include "util.h"
#include "higmac.h"
#include "ctrl.h"

#if (CONFIG_GMAC_NUMS == 2)
void higmac_set_macif(struct higmac_netdev_local *ld, int mode, int speed)
{
	unsigned int p = HIGMAC_PERI_CRG_IO_BASE;
	unsigned long v;

	/* enable change: port_mode */
	higmac_writel_bits(ld, 1, MODE_CHANGE_EN, BIT_MODE_CHANGE_EN);
	if (speed == 2)/* FIXME */
		speed = 5;/* 1000M */
	higmac_writel_bits(ld, speed, PORT_MODE, BITS_PORT_MODE);
	/* disable change: port_mode */
	higmac_writel_bits(ld, 0, MODE_CHANGE_EN, BIT_MODE_CHANGE_EN);

	/* soft reset mac_if */
	v = readl(p + RESET_CTRL);
	v |= 1 << (ld->index + 10);/* bit10 mac_if0 */
	writel(v, p + RESET_CTRL);

	/* config mac_if */
	if (ld->index)/* eth1 */
		writel(mode, HIGMAC_MACIF1_CTRL);
	else
		writel(mode, HIGMAC_MACIF0_CTRL);

	v = readl(p + RESET_CTRL);
	v &= ~(1 << (ld->index + 10));/* undo reset */
	writel(v, p + RESET_CTRL);
}
#else
void higmac_set_macif(struct higmac_netdev_local *ld, int mode, int speed)
{
	unsigned int p = HIGMAC_PERI_CRG_IO_BASE;
	unsigned long v;

	/* enable change: port_mode */
	higmac_writel_bits(ld, 1, MODE_CHANGE_EN, BIT_MODE_CHANGE_EN);
	if (speed == 2)/* FIXME */
		speed = 5;/* 1000M */
	higmac_writel_bits(ld, speed, PORT_MODE, BITS_PORT_MODE);
	/* disable change: port_mode */
	higmac_writel_bits(ld, 0, MODE_CHANGE_EN, BIT_MODE_CHANGE_EN);

	/* soft reset mac_if */
	v = readl(p + CRG_GMAC);
	v |= (1 << 2);
	writel(v, p + CRG_GMAC);

	/* config mac_if */
	writel(mode, HIGMAC_PERI_CRG_IO_BASE + SYSCTL_MAC_IF);

	v = readl(p + CRG_GMAC);
	v &= ~(1 << 2);/* undo reset */
	writel(v, p + CRG_GMAC);
}
#endif

int higmac_hw_set_macaddress(struct higmac_netdev_local *ld, unsigned char *mac)
{
	unsigned long reg;

	reg = mac[1] | (mac[0] << 8);
	higmac_writel(ld, reg, STATION_ADDR_HIGH);

	reg = mac[5] | (mac[4] << 8) | (mac[3] << 16) | (mac[2] << 24);
	higmac_writel(ld, reg, STATION_ADDR_LOW);

	return 0;
}

int higmac_hw_get_macaddress(struct higmac_netdev_local *ld, unsigned char *mac)
{
	unsigned long reg;

	reg = higmac_readl(ld, STATION_ADDR_HIGH);
	mac[0] = (reg>>8) & 0xff;
	mac[1] = reg & 0xff;

	reg = higmac_readl(ld, STATION_ADDR_LOW);
	mac[2] = (reg>>24) & 0xff;
	mac[3] = (reg>>16) & 0xff;
	mac[4] = (reg>>8) & 0xff;
	mac[5] = reg & 0xff;

	return 0;
}

static inline int _higmac_read_irqstatus(struct higmac_netdev_local *ld)
{
	int status;

	status = higmac_readl(ld, STATUS_PMU_INT);

	return status;
}

int higmac_clear_irqstatus(struct higmac_netdev_local *ld, int irqs)
{
	int status;

	higmac_writel(ld, irqs, RAW_PMU_INT);
	status = _higmac_read_irqstatus(ld);

	return status;
}

/*FIXME*/
int higmac_glb_preinit_dummy(struct higmac_netdev_local *ld)
{
	/* drop packet enable */
	higmac_writel(ld, 0x3F, REC_FILT_CONTROL);

	/*clear all interrupt status*/
	higmac_clear_irqstatus(ld, RAW_INT_ALL_MASK);

	/* disable interrupts */
	higmac_writel(ld, ~RAW_INT_ALL_MASK, ENA_PMU_INT);

	return 0;
}

#if (CONFIG_GMAC_NUMS == 2)
/* TODO: power on gmac here */
void higmac_sys_init(void)
{
	unsigned long p = 0;
	volatile unsigned int v = 0;

	/*soft reset*/
	p = (unsigned long)(HIGMAC_PERI_CRG_IO_BASE);

	v = readl(p + RESET_CTRL);
	v |= 0x0f3f; /* reset g1, g0, mac_if1, mac_if0 */
	writel(v, p + RESET_CTRL);

	udelay(100);

	v = readl(p + RESET_CTRL);
	v = 0x3f; /* undo reset */
	writel(v, p + RESET_CTRL);
	/* reset internal FEPHY */
	v = 0x1; /* use 25MHz clk, enable clk, undo reset */
	writel(v, p + INTERNAL_FEPHY_CTRL);

	udelay(100);

	v = readl(p + INTERNAL_FEPHY_CTRL);
	v |= (0x1 << 4); /* reset */
	writel(v, p + INTERNAL_FEPHY_CTRL);

	udelay(100);

	v = readl(p + INTERNAL_FEPHY_CTRL);
	v &= ~(0x1 << 4); /* undo reset */
	writel(v, p + INTERNAL_FEPHY_CTRL);
}
#else
void higmac_sys_init(void)
{
	unsigned long p = 0;
	volatile unsigned int v = 0;

	/*soft reset*/
	p = (unsigned long)(HIGMAC_PERI_CRG_IO_BASE);

	/* phy clk select */
	v = readl(p + CRG_GMAC);
	v |= (0x1 << 6);
	writel(v, p + CRG_GMAC);

/*
 * reset external phy
 * use GPIO0_1 to reset external phy
 */
#ifdef	HIGAMC_USE_GPIO_RESET_PHY
	/* Set Direction output */
	v = readl(HIGMAC_RESET_GPIO_BASE + HIGMAC_RESET_GPIO_DIR_OFS);
	v |= HIGMAC_RESET_GPIO_DIR_OUT;
	writel(v, HIGMAC_RESET_GPIO_BASE + HIGMAC_RESET_GPIO_DIR_OFS);

	/* Set GPIO0_1=1 */
	writel(HIGMAC_RESET_GPIO_VALUE, HIGMAC_RESET_GPIO_BASE
			+ HIGMAC_RESET_GPIO_DATA_OFS);
	udelay(50000);
	/* Set GPIO0_1=0 to reset phy */
	writel(~HIGMAC_RESET_GPIO_VALUE, HIGMAC_RESET_GPIO_BASE
			+ HIGMAC_RESET_GPIO_DATA_OFS);
	udelay(200000);

	/* Set GPIO0_1=1 to cancel reset phy */
	writel(HIGMAC_RESET_GPIO_VALUE, HIGMAC_RESET_GPIO_BASE
			+ HIGMAC_RESET_GPIO_DATA_OFS);
	udelay(50000);
#endif
	v = readl(p + CRG_GMAC);
	v |= 0x2; /* enable clk */
	writel(v, p + CRG_GMAC);

	v = readl(p + CRG_GMAC);
	v |= 0x1; /* reset */
	writel(v, p + CRG_GMAC);

	udelay(100);

	v = readl(p + CRG_GMAC);
	v &= ~(0x1); /*undo reset */
	writel(v, p + CRG_GMAC);
}
#endif

void higmac_sys_exit(void)
{

}

void higmac_sys_allstop(void)
{

}

int higmac_set_hwq_depth(struct higmac_netdev_local *ld)
{
	if (HIGMAC_HWQ_RX_FQ_DEPTH > HIGMAC_MAX_QUEUE_DEPTH) {
		BUG();
		return -1;
	}

	higmac_writel_bits(ld, 1, RX_FQ_REG_EN, \
		BITS_RX_FQ_DEPTH_EN);

	higmac_writel_bits(ld, HIGMAC_HWQ_RX_FQ_DEPTH<<3, RX_FQ_DEPTH, \
		BITS_RX_FQ_DEPTH);

	higmac_writel_bits(ld, 0, RX_FQ_REG_EN, \
		BITS_RX_FQ_DEPTH_EN);

	if (HIGMAC_HWQ_RX_BQ_DEPTH > HIGMAC_MAX_QUEUE_DEPTH) {
		BUG();
		return -1;
	}

	higmac_writel_bits(ld, 1, RX_BQ_REG_EN, \
		BITS_RX_BQ_DEPTH_EN);

	higmac_writel_bits(ld, HIGMAC_HWQ_RX_BQ_DEPTH<<3, RX_BQ_DEPTH, \
		BITS_RX_BQ_DEPTH);

	higmac_writel_bits(ld, 0, RX_BQ_REG_EN, \
		BITS_RX_BQ_DEPTH_EN);

	if (HIGMAC_HWQ_TX_BQ_DEPTH > HIGMAC_MAX_QUEUE_DEPTH) {
		BUG();
		return -1;
	}

	higmac_writel_bits(ld, 1, TX_BQ_REG_EN, \
		BITS_TX_BQ_DEPTH_EN);

	higmac_writel_bits(ld, HIGMAC_HWQ_TX_BQ_DEPTH<<3, TX_BQ_DEPTH, \
		BITS_TX_BQ_DEPTH);

	higmac_writel_bits(ld, 0, TX_BQ_REG_EN, \
		BITS_TX_BQ_DEPTH_EN);

	if (HIGMAC_HWQ_TX_RQ_DEPTH > HIGMAC_MAX_QUEUE_DEPTH) {
		BUG();
		return -1;
	}

	higmac_writel_bits(ld, 1, TX_RQ_REG_EN, \
		BITS_TX_RQ_DEPTH_EN);

	higmac_writel_bits(ld, HIGMAC_HWQ_TX_RQ_DEPTH<<3, TX_RQ_DEPTH, \
		BITS_TX_RQ_DEPTH);

	higmac_writel_bits(ld, 0, TX_RQ_REG_EN, \
		BITS_TX_RQ_DEPTH_EN);

	return 0;
}

int higmac_set_rx_fq_hwq_addr(struct higmac_netdev_local *ld,
		unsigned int phy_addr)
{
	higmac_writel_bits(ld, 1, RX_FQ_REG_EN, \
		BITS_RX_FQ_START_ADDR_EN);

	higmac_writel(ld, phy_addr, RX_FQ_START_ADDR);

	higmac_writel_bits(ld, 0, RX_FQ_REG_EN, \
		BITS_RX_FQ_START_ADDR_EN);

	return 0;
}

int higmac_set_rx_bq_hwq_addr(struct higmac_netdev_local *ld,
		unsigned int phy_addr)
{
	higmac_writel_bits(ld, 1, RX_BQ_REG_EN, \
		BITS_RX_BQ_START_ADDR_EN);

	higmac_writel(ld, phy_addr, RX_BQ_START_ADDR);

	higmac_writel_bits(ld, 0, RX_BQ_REG_EN, \
		BITS_RX_BQ_START_ADDR_EN);

	return 0;
}

int higmac_set_tx_bq_hwq_addr(struct higmac_netdev_local *ld,
		unsigned int phy_addr)
{
	higmac_writel_bits(ld, 1, TX_BQ_REG_EN, \
		BITS_TX_BQ_START_ADDR_EN);

	higmac_writel(ld, phy_addr, TX_BQ_START_ADDR);

	higmac_writel_bits(ld, 0, TX_BQ_REG_EN, \
		BITS_TX_BQ_START_ADDR_EN);

	return 0;
}

int higmac_set_tx_rq_hwq_addr(struct higmac_netdev_local *ld,
		unsigned int phy_addr)
{
	higmac_writel_bits(ld, 1, TX_RQ_REG_EN, \
		BITS_TX_RQ_START_ADDR_EN);

	higmac_writel(ld, phy_addr, TX_RQ_START_ADDR);

	higmac_writel_bits(ld, 0, TX_RQ_REG_EN, \
		BITS_TX_RQ_START_ADDR_EN);

	return 0;
}

int higmac_desc_enable(struct higmac_netdev_local *ld, int desc_ena)
{
	int old;

	old = higmac_readl(ld, DESC_WR_RD_ENA);
	higmac_writel(ld, old | desc_ena, DESC_WR_RD_ENA);

	return old;
}

int higmac_desc_disable(struct higmac_netdev_local *ld, int desc_dis)
{
	int old;

	old = higmac_readl(ld, DESC_WR_RD_ENA);
	higmac_writel(ld, old & (~desc_dis), DESC_WR_RD_ENA);

	return old;
}

void higmac_desc_flush(struct higmac_netdev_local *ld)
{
	higmac_writel_bits(ld, 1, STOP_CMD, BITS_TX_STOP_EN);
	while (higmac_readl_bits(ld, FLUSH_CMD, BITS_TX_FLUSH_FLAG) != 1)
		;
	higmac_writel_bits(ld, 1, FLUSH_CMD, BITS_TX_FLUSH_CMD);
	while (higmac_readl_bits(ld, FLUSH_CMD, BITS_TX_FLUSH_FLAG) != 0)
		;
	higmac_writel_bits(ld, 0, FLUSH_CMD, BITS_TX_FLUSH_CMD);
	higmac_writel_bits(ld, 0, STOP_CMD, BITS_TX_STOP_EN);

	higmac_writel_bits(ld, 1, STOP_CMD, BITS_RX_STOP_EN);
	while (higmac_readl_bits(ld, FLUSH_CMD, BITS_RX_FLUSH_FLAG) != 1)
		;
	higmac_writel_bits(ld, 1, FLUSH_CMD, BITS_RX_FLUSH_CMD);
	while (higmac_readl_bits(ld, FLUSH_CMD, BITS_RX_FLUSH_FLAG) != 0)
		;
	higmac_writel_bits(ld, 0, FLUSH_CMD, BITS_RX_FLUSH_CMD);
	higmac_writel_bits(ld, 0, STOP_CMD, BITS_RX_STOP_EN);
}
