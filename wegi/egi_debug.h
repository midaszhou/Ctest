#ifndef __EGI_DEBUG_H__
#define __EGI_DEBUG_H__

#include <stdio.h>


//#define EGI_DEBUG

/* debug flags */
#define	DBG_EGI 	(1<<0)
#define	DBG_TXT		(1<<1)
#define DBG_BTN		(1<<2)
#define DBG_PAGE	(1<<3)
#define DBG_COLOR	(1<<4)
#define DBG_SYMBOL	(1<<5)

/* default debug flags */
#define DEFAULT_DBG_FLAGS   (DBG_COLOR)  //  (DBG_PAGE | DBG_PAGE)  //(DBG_EGI | DBG_TXT | DBG_BTN | DBG_PAGE)


#ifdef EGI_DEBUG

	#define PDEBUG(fmt, args...)   fprintf(stderr,fmt, ## args)

#else
	#define PDEBUG(fmt,args...)

#endif


/* define egi_pdebug() */
#define egi_pdebug(flags, fmt, args...)			\
	do {						\
		if( flags & DEFAULT_DBG_FLAGS)		\
		{					\
			fprintf(stderr,fmt, ## args);	\
		}					\
	} while(0)




#endif
