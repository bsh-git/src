/*	$NetBSD: rgephy.c,v 1.57 2019/10/11 09:29:04 msaitoh Exp $	*/

/*
 * Copyright (c) 2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rgephy.c,v 1.57 2019/10/11 09:29:04 msaitoh Exp $");


/*
 * Driver for the RealTek 8169S/8110S internal 10/100/1000 PHY.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/socket.h>


#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/mdio.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#include <dev/mii/rgephyreg.h>

#include <dev/ic/rtl81x9reg.h>

static int	rgephy_match(device_t, cfdata_t, void *);
static void	rgephy_attach(device_t, device_t, void *);

struct rgephy_softc {
	struct mii_softc mii_sc;
	bool mii_no_rx_delay;
};

CFATTACH_DECL_NEW(rgephy, sizeof(struct rgephy_softc),
    rgephy_match, rgephy_attach, mii_phy_detach, mii_phy_activate);


static int	rgephy_service(struct mii_softc *, struct mii_data *, int);
static void	rgephy_status(struct mii_softc *);
static int	rgephy_mii_phy_auto(struct mii_softc *);
static void	rgephy_reset(struct mii_softc *);
static bool	rgephy_linkup(struct mii_softc *);
static void	rgephy_loop(struct mii_softc *);
static void	rgephy_load_dspcode(struct mii_softc *);

static const struct mii_phy_funcs rgephy_funcs = {
	rgephy_service, rgephy_status, rgephy_reset,
};

static const struct mii_phydesc rgephys[] = {
	MII_PHY_DESC(xxREALTEK, RTL8169S),
	MII_PHY_DESC(REALTEK, RTL8169S),
	MII_PHY_DESC(REALTEK, RTL8251),
	MII_PHY_END,
};

static int
rgephy_match(device_t parent, cfdata_t match, void *aux)
{
	struct mii_attach_args *ma = aux;

	if (mii_phy_match(ma, rgephys) != NULL)
		return 10;

	return 0;
}

static void
rgephy_attach(device_t parent, device_t self, void *aux)
{
	struct rgephy_softc *rsc = device_private(self);
	prop_dictionary_t prop = device_properties(self);
	struct mii_softc *sc = &rsc->mii_sc;
	struct mii_attach_args *ma = aux;
	struct mii_data *mii = ma->mii_data;
	const struct mii_phydesc *mpd;
	int rev;
	const char *sep = "";

	rev = MII_REV(ma->mii_id2);
	mpd = mii_phy_match(ma, rgephys);
	aprint_naive(": Media interface\n");

	sc->mii_dev = self;
	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_mpd_oui = MII_OUI(ma->mii_id1, ma->mii_id2);
	sc->mii_mpd_model = MII_MODEL(ma->mii_id2);
	sc->mii_mpd_rev = MII_REV(ma->mii_id2);

	if (sc->mii_mpd_model == MII_MODEL_REALTEK_RTL8169S) {
		aprint_normal(": RTL8211");
		if (sc->mii_mpd_rev != 0)
			aprint_normal("%c",'@' + sc->mii_mpd_rev);
		aprint_normal(" 1000BASE-T media interface\n");
	} else
		aprint_normal(": %s, rev. %d\n", mpd->mpd_name, rev);

	sc->mii_pdata = mii;
	sc->mii_flags = ma->mii_flags;
	sc->mii_anegticks = MII_ANEGTICKS_GIGE;

	sc->mii_funcs = &rgephy_funcs;

	prop_dictionary_get_bool(prop, "no-rx-delay", &rsc->mii_no_rx_delay);

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)
#define	PRINT(n)	aprint_normal("%s%s", sep, (n)); sep = ", "

#ifdef __FreeBSD__
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_LOOP, sc->mii_inst),
	    BMCR_LOOP | BMCR_S100);
#endif

	PHY_READ(sc, MII_BMSR, &sc->mii_capabilities);
	sc->mii_capabilities &= ma->mii_capmask;
	sc->mii_capabilities &= ~BMSR_ANEG;

	/*
	 * FreeBSD does not check EXSTAT, but instead adds gigabit
	 * media explicitly. Why?
	 */
	aprint_normal_dev(self, "");
	if (sc->mii_capabilities & BMSR_EXTSTAT)
		PHY_READ(sc, MII_EXTSR, &sc->mii_extcapabilities);

	mii_phy_add_media(sc);

	/* rtl8169S does not report auto-sense; add manually.  */
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, sc->mii_inst), MII_NMEDIA);
	sep =", ";
	PRINT("auto");

#undef	ADD
#undef	PRINT

	rgephy_reset(sc);
	aprint_normal("\n");
}

static int
rgephy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	uint16_t reg, speed, gig, anar;

	switch (cmd) {
	case MII_POLLSTAT:
		/* If we're not polling our PHY instance, just return. */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return 0;
		break;

	case MII_MEDIACHG:
		/*
		 * If the media indicates a different PHY instance,
		 * isolate ourselves.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst) {
			PHY_READ(sc, MII_BMCR, &reg);
			PHY_WRITE(sc, MII_BMCR, reg | BMCR_ISO);
			return 0;
		}

		/* If the interface is not up, don't do anything. */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		rgephy_reset(sc);	/* XXX hardware bug work-around */

		PHY_READ(sc, MII_ANAR, &anar);
		anar &= ~(ANAR_TX_FD | ANAR_TX | ANAR_10_FD | ANAR_10);

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
#ifdef foo
			/* If we're already in auto mode, just return. */
			PHY_READ(sc, MII_BMCR, &reg);
			if (reg & BMCR_AUTOEN)
				return 0;
#endif
			(void)rgephy_mii_phy_auto(sc);
			break;
		case IFM_1000_T:
			speed = BMCR_S1000;
			goto setit;
		case IFM_100_TX:
			speed = BMCR_S100;
			anar |= ANAR_TX_FD | ANAR_TX;
			goto setit;
		case IFM_10_T:
			speed = BMCR_S10;
			anar |= ANAR_10_FD | ANAR_10;
 setit:
			rgephy_loop(sc);
			if ((ife->ifm_media & IFM_FDX) != 0) {
				speed |= BMCR_FDX;
				gig = GTCR_ADV_1000TFDX;
				anar &= ~(ANAR_TX | ANAR_10);
			} else {
				gig = GTCR_ADV_1000THDX;
				anar &= ~(ANAR_TX_FD | ANAR_10_FD);
			}

			if (IFM_SUBTYPE(ife->ifm_media) != IFM_1000_T) {
				PHY_WRITE(sc, MII_100T2CR, 0);
				PHY_WRITE(sc, MII_ANAR, anar);
				PHY_WRITE(sc, MII_BMCR,
				    speed | BMCR_AUTOEN | BMCR_STARTNEG);
				break;
			}

			/*
			 * When setting the link manually, one side must be the
			 * master and the other the slave. However ifmedia
			 * doesn't give us a good way to specify this, so we
			 * fake it by using one of the LINK flags. If LINK0 is
			 * set, we program the PHY to be a master, otherwise
			 * it's a slave.
			 */
			if ((mii->mii_ifp->if_flags & IFF_LINK0)) {
				PHY_WRITE(sc, MII_100T2CR,
				    gig | GTCR_MAN_MS | GTCR_ADV_MS);
			} else
				PHY_WRITE(sc, MII_100T2CR, gig | GTCR_MAN_MS);
			PHY_WRITE(sc, MII_BMCR,
			    speed | BMCR_AUTOEN | BMCR_STARTNEG);
			break;
		case IFM_NONE:
			PHY_WRITE(sc, MII_BMCR, BMCR_ISO | BMCR_PDOWN);
			break;
		case IFM_100_T4:
		default:
			return EINVAL;
		}
		break;

	case MII_TICK:
		/* If we're not currently selected, just return. */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return 0;

		/* Is the interface even up? */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			return 0;

		/* Only used for autonegotiation. */
		if ((IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO) &&
		    (IFM_SUBTYPE(ife->ifm_media) != IFM_1000_T)) {
			/*
			 * Reset autonegotiation timer to 0 to make sure
			 * the future autonegotiation start with 0.
			 */
			sc->mii_ticks = 0;
			break;
		}

		/*
		 * Check to see if we have link.  If we do, we don't
		 * need to restart the autonegotiation process.  Read
		 * the BMSR twice in case it's latched.
		 */
		if (rgephy_linkup(sc)) {
			sc->mii_ticks = 0;
			break;
		}

		/* Announce link loss right after it happens. */
		if (sc->mii_ticks++ == 0)
			break;

		/* Only retry autonegotiation every mii_anegticks seconds. */
		if (sc->mii_ticks <= sc->mii_anegticks)
			return 0;

		rgephy_mii_phy_auto(sc);
		break;
	}

	/* Update the media status. */
	rgephy_status(sc);

	/*
	 * Callback if something changed. Note that we need to poke
	 * the DSP on the RealTek PHYs if the media changes.
	 */
	if (sc->mii_media_active != mii->mii_media_active ||
	    sc->mii_media_status != mii->mii_media_status ||
	    cmd == MII_MEDIACHG) {
		rgephy_load_dspcode(sc);
	}
	mii_phy_update(sc, cmd);
	return 0;
}

static bool
rgephy_linkup(struct mii_softc *sc)
{
	bool linkup = false;
	uint16_t reg;

	if (sc->mii_mpd_rev >= RGEPHY_8211F) {
		PHY_READ(sc, RGEPHY_MII_PHYSR, &reg);
		if (reg & RGEPHY_PHYSR_LINK)
			linkup = true;
	} else if (sc->mii_mpd_rev >= RGEPHY_8211B) {
		PHY_READ(sc, RGEPHY_MII_SSR, &reg);
		if (reg & RGEPHY_SSR_LINK)
			linkup = true;
	} else {
		PHY_READ(sc, RTK_GMEDIASTAT, &reg);
		if ((reg & RTK_GMEDIASTAT_LINK) != 0)
			linkup = true;
	}

	return linkup;
}

static void
rgephy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	uint16_t gstat, bmsr, bmcr, gtsr, physr, ssr;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	if (rgephy_linkup(sc))
		mii->mii_media_status |= IFM_ACTIVE;

	PHY_READ(sc, MII_BMSR, &bmsr);
	PHY_READ(sc, MII_BMCR, &bmcr);

	if ((bmcr & BMCR_ISO) != 0) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
		return;
	}

	if ((bmcr & BMCR_LOOP) != 0)
		mii->mii_media_active |= IFM_LOOP;

	if ((bmcr & BMCR_AUTOEN) != 0) {
		if ((bmsr & BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}
	}

	if (sc->mii_mpd_rev >= RGEPHY_8211F) {
		PHY_READ(sc, RGEPHY_MII_PHYSR, &physr);
		switch (__SHIFTOUT(physr, RGEPHY_PHYSR_SPEED)) {
		case RGEPHY_PHYSR_SPEED_1000:
			mii->mii_media_active |= IFM_1000_T;
			break;
		case RGEPHY_PHYSR_SPEED_100:
			mii->mii_media_active |= IFM_100_TX;
			break;
		case RGEPHY_PHYSR_SPEED_10:
			mii->mii_media_active |= IFM_10_T;
			break;
		default:
			mii->mii_media_active |= IFM_NONE;
			break;
		}
		if (physr & RGEPHY_PHYSR_DUPLEX)
			mii->mii_media_active |= mii_phy_flowstatus(sc) |
			    IFM_FDX;
		else
			mii->mii_media_active |= IFM_HDX;
	} else if (sc->mii_mpd_rev >= RGEPHY_8211B) {
		PHY_READ(sc, RGEPHY_MII_SSR, &ssr);
		switch (ssr & RGEPHY_SSR_SPD_MASK) {
		case RGEPHY_SSR_S1000:
			mii->mii_media_active |= IFM_1000_T;
			break;
		case RGEPHY_SSR_S100:
			mii->mii_media_active |= IFM_100_TX;
			break;
		case RGEPHY_SSR_S10:
			mii->mii_media_active |= IFM_10_T;
			break;
		default:
			mii->mii_media_active |= IFM_NONE;
			break;
		}
		if (ssr & RGEPHY_SSR_FDX)
			mii->mii_media_active |= mii_phy_flowstatus(sc) |
			    IFM_FDX;
		else
			mii->mii_media_active |= IFM_HDX;
	} else {
		PHY_READ(sc, RTK_GMEDIASTAT, &gstat);
		if ((gstat & RTK_GMEDIASTAT_1000MBPS) != 0)
			mii->mii_media_active |= IFM_1000_T;
		else if ((gstat & RTK_GMEDIASTAT_100MBPS) != 0)
			mii->mii_media_active |= IFM_100_TX;
		else if ((gstat & RTK_GMEDIASTAT_10MBPS) != 0)
			mii->mii_media_active |= IFM_10_T;
		else
			mii->mii_media_active |= IFM_NONE;
		if ((gstat & RTK_GMEDIASTAT_FDX) != 0)
			mii->mii_media_active |= mii_phy_flowstatus(sc) |
			    IFM_FDX;
		else
			mii->mii_media_active |= IFM_HDX;
	}

	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_T) {
		PHY_READ(sc, MII_GTSR, &gtsr);
		if ((gtsr & GTSR_MS_RES) != 0)
			mii->mii_media_active |= IFM_ETH_MASTER;
	}
}

static int
rgephy_mii_phy_auto(struct mii_softc *mii)
{
	int anar;

	mii->mii_ticks = 0;
	rgephy_loop(mii);
	rgephy_reset(mii);

	anar = BMSR_MEDIA_TO_ANAR(mii->mii_capabilities) | ANAR_CSMA;
	if (mii->mii_flags & MIIF_DOPAUSE)
		anar |= ANAR_FC | ANAR_PAUSE_ASYM;

	PHY_WRITE(mii, MII_ANAR, anar);
	DELAY(1000);
	PHY_WRITE(mii, MII_100T2CR, GTCR_ADV_1000THDX | GTCR_ADV_1000TFDX);
	DELAY(1000);
	PHY_WRITE(mii, MII_BMCR, BMCR_AUTOEN | BMCR_STARTNEG);
	DELAY(100);

	return EJUSTRETURN;
}

static void
rgephy_loop(struct mii_softc *sc)
{
	uint16_t bmsr;
	int i;

	if (sc->mii_mpd_model != MII_MODEL_REALTEK_RTL8251 &&
	    sc->mii_mpd_rev < RGEPHY_8211B) {
		PHY_WRITE(sc, MII_BMCR, BMCR_PDOWN);
		DELAY(1000);
	}

	for (i = 0; i < 15000; i++) {
		PHY_READ(sc, MII_BMSR, &bmsr);
		if ((bmsr & BMSR_LINK) == 0) {
#if 0
			device_printf(sc->mii_dev, "looped %d\n", i);
#endif
			break;
		}
		DELAY(10);
	}
}

static inline int
PHY_SETBIT(struct mii_softc *sc, int y, uint16_t z)
{
	uint16_t _tmp;
	int rv;

	if ((rv = PHY_READ(sc, y, &_tmp)) != 0)
		return rv;
	return PHY_WRITE(sc, y, _tmp | z);
}

static inline int
PHY_CLRBIT(struct mii_softc *sc, int y, uint16_t z)
{
	uint16_t _tmp;
	int rv;

	if ((rv = PHY_READ(sc, y, &_tmp)) != 0)
	    return rv;
	return PHY_WRITE(sc, y, _tmp & ~z);
}

/*
 * Initialize RealTek PHY per the datasheet. The DSP in the PHYs of existing
 * revisions of the 8169S/8110S chips need to be tuned in order to reliably
 * negotiate a 1000Mbps link. This is only needed for rev 0 and rev 1 of the
 * PHY. Later versions work without any fixups.
 */
static void
rgephy_load_dspcode(struct mii_softc *sc)
{
	uint16_t val;

	if (sc->mii_mpd_model == MII_MODEL_REALTEK_RTL8251 ||
	    sc->mii_mpd_rev >= RGEPHY_8211B)
		return;

#if 1
	PHY_WRITE(sc, 31, 0x0001);
	PHY_WRITE(sc, 21, 0x1000);
	PHY_WRITE(sc, 24, 0x65C7);
	PHY_CLRBIT(sc, 4, 0x0800);
	PHY_READ(sc, 4, &val);
	val &= 0xFFF;
	PHY_WRITE(sc, 4, val);
	PHY_WRITE(sc, 3, 0x00A1);
	PHY_WRITE(sc, 2, 0x0008);
	PHY_WRITE(sc, 1, 0x1020);
	PHY_WRITE(sc, 0, 0x1000);
	PHY_SETBIT(sc, 4, 0x0800);
	PHY_CLRBIT(sc, 4, 0x0800);
	PHY_READ(sc, 4, &val);
	val = (val & 0xFFF) | 0x7000;
	PHY_WRITE(sc, 4, val);
	PHY_WRITE(sc, 3, 0xFF41);
	PHY_WRITE(sc, 2, 0xDE60);
	PHY_WRITE(sc, 1, 0x0140);
	PHY_WRITE(sc, 0, 0x0077);
	PHY_READ(sc, 4, &val);
	val = (val & 0xFFF) | 0xA000;
	PHY_WRITE(sc, 4, val);
	PHY_WRITE(sc, 3, 0xDF01);
	PHY_WRITE(sc, 2, 0xDF20);
	PHY_WRITE(sc, 1, 0xFF95);
	PHY_WRITE(sc, 0, 0xFA00);
	PHY_READ(sc, 4, &val);
	val = (val & 0xFFF) | 0xB000;
	PHY_WRITE(sc, 4, val);
	PHY_WRITE(sc, 3, 0xFF41);
	PHY_WRITE(sc, 2, 0xDE20);
	PHY_WRITE(sc, 1, 0x0140);
	PHY_WRITE(sc, 0, 0x00BB);
	PHY_READ(sc, 4, &val);
	val = (val & 0xFFF) | 0xF000;
	PHY_WRITE(sc, 4, val);
	PHY_WRITE(sc, 3, 0xDF01);
	PHY_WRITE(sc, 2, 0xDF20);
	PHY_WRITE(sc, 1, 0xFF95);
	PHY_WRITE(sc, 0, 0xBF00);
	PHY_SETBIT(sc, 4, 0x0800);
	PHY_CLRBIT(sc, 4, 0x0800);
	PHY_WRITE(sc, 31, 0x0000);
#else
	(void)val;
	PHY_WRITE(sc, 0x1f, 0x0001);
	PHY_WRITE(sc, 0x15, 0x1000);
	PHY_WRITE(sc, 0x18, 0x65c7);
	PHY_WRITE(sc, 0x04, 0x0000);
	PHY_WRITE(sc, 0x03, 0x00a1);
	PHY_WRITE(sc, 0x02, 0x0008);
	PHY_WRITE(sc, 0x01, 0x1020);
	PHY_WRITE(sc, 0x00, 0x1000);
	PHY_WRITE(sc, 0x04, 0x0800);
	PHY_WRITE(sc, 0x04, 0x0000);
	PHY_WRITE(sc, 0x04, 0x7000);
	PHY_WRITE(sc, 0x03, 0xff41);
	PHY_WRITE(sc, 0x02, 0xde60);
	PHY_WRITE(sc, 0x01, 0x0140);
	PHY_WRITE(sc, 0x00, 0x0077);
	PHY_WRITE(sc, 0x04, 0x7800);
	PHY_WRITE(sc, 0x04, 0x7000);
	PHY_WRITE(sc, 0x04, 0xa000);
	PHY_WRITE(sc, 0x03, 0xdf01);
	PHY_WRITE(sc, 0x02, 0xdf20);
	PHY_WRITE(sc, 0x01, 0xff95);
	PHY_WRITE(sc, 0x00, 0xfa00);
	PHY_WRITE(sc, 0x04, 0xa800);
	PHY_WRITE(sc, 0x04, 0xa000);
	PHY_WRITE(sc, 0x04, 0xb000);
	PHY_WRITE(sc, 0x0e, 0xff41);
	PHY_WRITE(sc, 0x02, 0xde20);
	PHY_WRITE(sc, 0x01, 0x0140);
	PHY_WRITE(sc, 0x00, 0x00bb);
	PHY_WRITE(sc, 0x04, 0xb800);
	PHY_WRITE(sc, 0x04, 0xb000);
	PHY_WRITE(sc, 0x04, 0xf000);
	PHY_WRITE(sc, 0x03, 0xdf01);
	PHY_WRITE(sc, 0x02, 0xdf20);
	PHY_WRITE(sc, 0x01, 0xff95);
	PHY_WRITE(sc, 0x00, 0xbf00);
	PHY_WRITE(sc, 0x04, 0xf800);
	PHY_WRITE(sc, 0x04, 0xf000);
	PHY_WRITE(sc, 0x04, 0x0000);
	PHY_WRITE(sc, 0x1f, 0x0000);
	PHY_WRITE(sc, 0x0b, 0x0000);

#endif

	DELAY(40);
}

static void
rgephy_reset(struct mii_softc *sc)
{
	struct rgephy_softc *rsc = (struct rgephy_softc *)sc;
	uint16_t ssr, phycr1;

	mii_phy_reset(sc);
	DELAY(1000);

	if (sc->mii_mpd_model != MII_MODEL_REALTEK_RTL8251 &&
	    sc->mii_mpd_rev < RGEPHY_8211B) {
		rgephy_load_dspcode(sc);
	} else if (sc->mii_mpd_rev == RGEPHY_8211C) {
		/* RTL8211C(L) */
		PHY_READ(sc, RGEPHY_MII_SSR, &ssr);
		if ((ssr & RGEPHY_SSR_ALDPS) != 0) {
			ssr &= ~RGEPHY_SSR_ALDPS;
			PHY_WRITE(sc, RGEPHY_MII_SSR, ssr);
		}
	} else if (sc->mii_mpd_rev == RGEPHY_8211E) {
		/* RTL8211E */
		if (rsc->mii_no_rx_delay) {
			/* Disable RX internal delay (undocumented) */
			PHY_WRITE(sc, 0x1f, 0x0007);
			PHY_WRITE(sc, 0x1e, 0x00a4);
			PHY_WRITE(sc, 0x1c, 0xb591);
			PHY_WRITE(sc, 0x1f, 0x0000);
		}
	} else if (sc->mii_mpd_rev == RGEPHY_8211F) {
		/* RTL8211F */
		PHY_READ(sc, RGEPHY_MII_PHYCR1, &phycr1);
		phycr1 &= ~RGEPHY_PHYCR1_MDI_MMCE;
		phycr1 &= ~RGEPHY_PHYCR1_ALDPS_EN;
		PHY_WRITE(sc, RGEPHY_MII_PHYCR1, phycr1);
	} else {
		PHY_WRITE(sc, 0x1F, 0x0000);
		PHY_WRITE(sc, 0x0e, 0x0000);
	}

	/* Reset capabilities */
	/* Step1: write our capability */
	/* 10/100 capability */
	PHY_WRITE(sc, MII_ANAR,
	    ANAR_TX_FD | ANAR_TX | ANAR_10_FD | ANAR_10 | ANAR_CSMA);
	/* 1000 capability */
	PHY_WRITE(sc, MII_100T2CR, GTCR_ADV_1000TFDX | GTCR_ADV_1000THDX);

	/* Step2: Restart NWay */
	/* NWay enable and Restart NWay */
	PHY_WRITE(sc, MII_BMCR, BMCR_RESET | BMCR_AUTOEN | BMCR_STARTNEG);

	if (sc->mii_mpd_rev >= RGEPHY_8211D) {
		/* RTL8211F */
		delay(10000);
		/* disable EEE */
		MMD_INDIRECT_WRITE(sc, MDIO_MMD_AN | MMDACR_FN_DATA,
		    MDIO_AN_EEEADVERT, 0x0000);
	}
}
