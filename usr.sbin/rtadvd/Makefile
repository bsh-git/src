# $NetBSD: Makefile,v 1.23 2019/10/13 07:28:21 mrg Exp $

WARNS?=	4

.include <bsd.own.mk>

USE_FORT?=	yes	# network server

RUMPPRG=	rtadvd
SRCS=		rtadvd.c rrenum.c advcap.c if.c config.c timer.c dump.c
MAN=		rtadvd.8 rtadvd.conf.5

CPPFLAGS+=	-DINET6
LDADD+=		-lutil
DPADD+=		${LIBUTIL}

LIBWRAP=	${NETBSDSRCDIR}/lib/libwrap
SRCS+=		expandm.c
.PATH.c:	${LIBWRAP}
CPPFLAGS+=	-I${LIBWRAP}


.if ${MKSHARE} != "no"
FILESDIR=	/usr/share/examples/rtadvd
FILES=	rtadvd.conf
.endif

.if (${MKRUMP} != "no")
CPPFLAGS+=	-DRUMP_ACTION
LDADD.rump+=	-lrumpres
DPADD.rump+=	${LIBRUMPRES}
.endif

CWARNFLAGS.clang+=	-Wno-error=address-of-packed-member

COPTS.if.c+=		${GCC_NO_STRINGOP_TRUNCATION}
COPTS.config.c+=	${GCC_NO_STRINGOP_TRUNCATION}

.include <bsd.prog.mk>
