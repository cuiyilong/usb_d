/*
 * USB PHY defines
 *
 * These APIs may be used between USB controllers.  USB device drivers
 * (for either host or peripheral roles) don't use these calls; they
 * continue to use just usb_device and usb_gadget.
 */

#ifndef __LINUX_USB_PHY_H
#define __LINUX_USB_PHY_H

#include <linux/notifier.h>
#include <linux/usb.h>

enum usb_phy_interface {
	USBPHY_INTERFACE_MODE_UNKNOWN,
	USBPHY_INTERFACE_MODE_UTMI,
	USBPHY_INTERFACE_MODE_UTMIW,
	USBPHY_INTERFACE_MODE_ULPI,
	USBPHY_INTERFACE_MODE_SERIAL,
	USBPHY_INTERFACE_MODE_HSIC,
};

enum usb_phy_events {
	USB_EVENT_NONE,         /* no events or cable disconnected */
	USB_EVENT_VBUS,         /* vbus valid event */
	USB_EVENT_ID,           /* id was grounded */
	USB_EVENT_CHARGER,      /* usb dedicated charger */
	USB_EVENT_ENUMERATED,   /* gadget driver enumerated */
};

/* associate a type with PHY */
enum usb_phy_type {
	USB_PHY_TYPE_UNDEFINED,
	USB_PHY_TYPE_USB2,
	USB_PHY_TYPE_USB3,
};

/* OTG defines lots of enumeration states before device reset */
enum usb_otg_state {
	OTG_STATE_UNDEFINED = 0,

	/* single-role peripheral, and dual-role default-b */
	OTG_STATE_B_IDLE,
	OTG_STATE_B_SRP_INIT,
	OTG_STATE_B_PERIPHERAL,

	/* extra dual-role default-b states */
	OTG_STATE_B_WAIT_ACON,
	OTG_STATE_B_HOST,

	/* dual-role default-a */
	OTG_STATE_A_IDLE,
	OTG_STATE_A_WAIT_VRISE,
	OTG_STATE_A_WAIT_BCON,
	OTG_STATE_A_HOST,
	OTG_STATE_A_SUSPEND,
	OTG_STATE_A_PERIPHERAL,
	OTG_STATE_A_WAIT_VFALL,
	OTG_STATE_A_VBUS_ERR,
};

struct usb_phy;
struct usb_otg;

/* for phys connected thru an ULPI interface, the user must
 * provide access ops
 */
struct usb_phy_io_ops {
	int (*read)(struct usb_phy *x, u32 reg);
	int (*write)(struct usb_phy *x, u32 val, u32 reg);
};

struct usb_phy {
	struct device		*dev;
	const char		*label;
	unsigned int		 flags;

	enum usb_phy_type	type;
	enum usb_phy_events	last_event;

	struct usb_otg		*otg;

	struct device		*io_dev;
	struct usb_phy_io_ops	*io_ops;
	void __iomem		*io_priv;

	/* for notification of usb_phy_events */
	struct atomic_notifier_head	notifier;

	/* to pass extra port status to the root hub */
	u16			port_status;
	u16			port_change;

	/* to support controllers that have multiple phys */
	struct list_head	head;

	/* initialize/shutdown the phy */
	int	(*init)(struct usb_phy *x);
	void	(*shutdown)(struct usb_phy *x);

	/* do additional settings after complete initialization */
	int	(*post_init)(struct usb_phy *x);

	/* enable/disable VBUS */
	int	(*set_vbus)(struct usb_phy *x, int on);

	/* effective for B devices, ignored for A-peripheral */
	int	(*set_power)(struct usb_phy *x,
				unsigned mA);

	/* Set phy into suspend mode */
	int	(*set_suspend)(struct usb_phy *x,
				int suspend);

	/*
	 * Set wakeup enable for PHY, in that case, the PHY can be
	 * woken up from suspend status due to external events,
	 * like vbus change, dp/dm change and id.
	 */
	int	(*set_wakeup)(struct usb_phy *x, bool enabled);

	/* notify phy connect status change */
	int	(*notify_connect)(struct usb_phy *x,
			enum usb_device_speed speed);
	int	(*notify_disconnect)(struct usb_phy *x,
			enum usb_device_speed speed);

	/* reset the PHY */
	int	(*reset_phy)(struct usb_phy *x);
	void	(*set_emphasis)(struct usb_phy *x, bool enabled);
};

/**
 * struct usb_phy_bind - represent the binding for the phy
 * @dev_name: the device name of the device that will bind to the phy
 * @phy_dev_name: the device name of the phy
 * @index: used if a single controller uses multiple phys
 * @phy: reference to the phy
 * @list: to maintain a linked list of the binding information
 */
struct usb_phy_bind {
	const char	*dev_name;
	const char	*phy_dev_name;
	u8		index;
	struct usb_phy	*phy;
	struct list_head list;
};

/* for board-specific init logic */
extern int usb_add_phy(struct usb_phy *, enum usb_phy_type type);
extern int usb_add_phy_dev(struct usb_phy *);
extern void usb_remove_phy(struct usb_phy *);

/* helpers for direct access thru low-level io interface */
static inline int usb_phy_io_read(struct usb_phy *x, u32 reg)
{
	if (x && x->io_ops && x->io_ops->read)
		return x->io_ops->read(x, reg);

	return -EINVAL;
}

static inline int usb_phy_io_write(struct usb_phy *x, u32 val, u32 reg)
{
	if (x && x->io_ops && x->io_ops->write)
		return x->io_ops->write(x, val, reg);

	return -EINVAL;
}

static inline int
usb_phy_init(struct usb_phy *x)
{
	if (x && x->init)
		return x->init(x);

	return 0;
}

static inline int
usb_phy_post_init(struct usb_phy *x)
{
	if (x && x->post_init)
		return x->post_init(x);

	return 0;
}

static inline void
usb_phy_shutdown(struct usb_phy *x)
{
	if (x && x->shutdown)
		x->shutdown(x);
}

static inline int
usb_phy_vbus_on(struct usb_phy *x)
{
	if (!x || !x->set_vbus)
		return 0;

	return x->set_vbus(x, true);
}

static inline int
usb_phy_vbus_off(struct usb_phy *x)
{
	if (!x || !x->set_vbus)
		return 0;

	return x->set_vbus(x, false);
}

static inline int usb_phy_reset(struct usb_phy *x)
{
	if (!x || !x->reset_phy)
		return 0;

	return x->reset_phy(x);
}

static inline void usb_phy_emphasis_set(struct usb_phy *x, bool enabled)
{
	if (!x || !x->set_emphasis)
		return;

	x->set_emphasis(x, enabled);
}

/* for usb host and peripheral controller drivers */
#if IS_ENABLED(CONFIG_USB_PHY)
extern struct usb_phy *usb_get_phy(enum usb_phy_type type);
extern struct usb_phy *devm_usb_get_phy(struct device *dev,
	enum usb_phy_type type);
extern struct usb_phy *usb_get_phy_dev(struct device *dev, u8 index);
extern struct usb_phy *devm_usb_get_phy_dev(struct device *dev, u8 index);
extern struct usb_phy *devm_usb_get_phy_by_phandle(struct device *dev,
	const char *phandle, u8 index);
extern struct usb_phy *devm_usb_get_phy_by_node(struct device *dev,
	struct device_node *node, struct notifier_block *nb);
extern void usb_put_phy(struct usb_phy *);
extern void devm_usb_put_phy(struct device *dev, struct usb_phy *x);
extern int usb_bind_phy(const char *dev_name, u8 index,
				const char *phy_dev_name);
extern void usb_phy_set_event(struct usb_phy *x, unsigned long event);
#else
static inline struct usb_phy *usb_get_phy(enum usb_phy_type type)
{
	return ERR_PTR(-ENXIO);
}

static inline struct usb_phy *devm_usb_get_phy(struct device *dev,
	enum usb_phy_type type)
{
	return ERR_PTR(-ENXIO);
}

static inline struct usb_phy *usb_get_phy_dev(struct device *dev, u8 index)
{
	return ERR_PTR(-ENXIO);
}

static inline struct usb_phy *devm_usb_get_phy_dev(struct device *dev, u8 index)
{
	return ERR_PTR(-ENXIO);
}

static inline struct usb_phy *devm_usb_get_phy_by_phandle(struct device *dev,
	const char *phandle, u8 index)
{
	return ERR_PTR(-ENXIO);
}

static inline struct usb_phy *devm_usb_get_phy_by_node(struct device *dev,
	struct device_node *node, struct notifier_block *nb)
{
	return ERR_PTR(-ENXIO);
}

static inline void usb_put_phy(struct usb_phy *x)
{
}

static inline void devm_usb_put_phy(struct device *dev, struct usb_phy *x)
{
}

static inline int usb_bind_phy(const char *dev_name, u8 index,
				const char *phy_dev_name)
{
	return -EOPNOTSUPP;
}

static inline void usb_phy_set_event(struct usb_phy *x, unsigned long event)
{
}
#endif

static inline int
usb_phy_set_power(struct usb_phy *x, unsigned mA)
{
	if (x && x->set_power)
		return x->set_power(x, mA);
	return 0;
}

/* Context: can sleep */
static inline int
usb_phy_set_suspend(struct usb_phy *x, int suspend)
{
	if (x && x->set_suspend != NULL)
		return x->set_suspend(x, suspend);
	else
		return 0;
}

static inline int
usb_phy_set_wakeup(struct usb_phy *x, bool enabled)
{
	if (x && x->set_wakeup)
		return x->set_wakeup(x, enabled);
	else
		return 0;
}

static inline int
usb_phy_notify_connect(struct usb_phy *x, enum usb_device_speed speed)
{
	if (x && x->notify_connect)
		return x->notify_connect(x, speed);
	else
		return 0;
}

static inline int
usb_phy_notify_disconnect(struct usb_phy *x, enum usb_device_speed speed)
{
	if (x && x->notify_disconnect)
		return x->notify_disconnect(x, speed);
	else
		return 0;
}

/* notifiers */
static inline int
usb_register_notifier(struct usb_phy *x, struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&x->notifier, nb);
}

static inline void
usb_unregister_notifier(struct usb_phy *x, struct notifier_block *nb)
{
	atomic_notifier_chain_unregister(&x->notifier, nb);
}

static inline const char *usb_phy_type_string(enum usb_phy_type type)
{
	switch (type) {
	case USB_PHY_TYPE_USB2:
		return "USB2 PHY";
	case USB_PHY_TYPE_USB3:
		return "USB3 PHY";
	default:
		return "UNKNOWN PHY TYPE";
	}
}
#endif /* __LINUX_USB_PHY_H */
