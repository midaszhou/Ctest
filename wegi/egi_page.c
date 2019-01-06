#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "list.h"
#include "xpt2046.h"
#include "egi.h"
#include "egi_timer.h"
#include "egi_page.h"
#include "egi_debug.h"
#include "egi_color.h"
#include "egi_symbol.h"
#include "bmpjpg.h"

/*


*/

/*---------------------------------------------
init a egi page
tag: a tag for the page
Return
	page pointer		ok
	NULL			fail
--------------------------------------------*/
EGI_PAGE * egi_page_new(char *tag)
{
	int i;
	EGI_PAGE *page;

	/* 2. malloc page */
	page=malloc(sizeof(struct egi_page));
	if(page == NULL)
	{
		printf("egi_page_init(): fail to malloc page.\n");
		return NULL;
	}
	/* clear data */
	memset(page,0,sizeof(struct egi_page));

	/* 3. malloc page->ebox */
	page->ebox=egi_ebox_new(type_page);
	if( page == NULL)
	{
		printf("egi_page_init(): egi_ebox_new() fails.\n");
		return NULL;
	}

	/* 4. put tag here */
	egi_ebox_settag(page->ebox,tag);
	//strncpy(page->ebox->tag,tag,EGI_TAG_LENGTH); /* EGI_TAG_LENGTH+1 for a EGI_EBOX */


	/* 5. init pthreads. ??? Not necessary. since alread memset() in above ??? */
	for(i=0;i<EGI_PAGE_MAXTHREADS;i++)
	{
		page->thread_running[i]=false;
		page->runner[i]=NULL; /* thread functions */
	}

	/* 6. init list */
        INIT_LIST_HEAD(&page->list_head);

	return page;
}


/*---------------------------------------------
free a egi page
Return:
	0	OK
	<0	fails
--------------------------------------------*/
int egi_page_free(EGI_PAGE *page)
{
	int ret=0;
	struct list_head *tnode, *tmpnode;
	EGI_EBOX *ebox;

	/* check data */
	if(page == NULL)
	{
		printf("egi_page_free(): page is NULL! fail to free.\n");
		return -1;
	}

	/*  free every child in list */
	if(!list_empty(&page->list_head))
	{
		/* traverse the list, not safe */
		list_for_each_safe(tnode, tmpnode, &page->list_head)
        	{
               	 	ebox=list_entry(tnode,EGI_EBOX,node);
			egi_pdebug(DBG_PAGE,"egi_page_free(): ebox '%s' is unlisted from page '%s' and freed.\n" 
									,ebox->tag,page->ebox->tag);
                	list_del(tnode);
                	ebox->free(ebox);
        	}
	}

	/* free self ebox */
	free(page->ebox);
	/* free page */
	free(page);

	return ret;
}


/*--------------------------------------------------
add a ebox into a page's head list.
return:
	0	OK
	<0	fails
---------------------------------------------------*/
int egi_page_addlist(EGI_PAGE *page, EGI_EBOX *ebox)
{
	/* check data */
	if(page==NULL || ebox==NULL)
	{
		printf("egi_page_travlist(): page or ebox is NULL!\n");
		return -1;
	}

	list_add_tail(&ebox->node, &page->list_head);
	egi_pdebug(DBG_PAGE,"egi_page_addlist(): ebox '%s' is added to page '%s' \n",
								ebox->tag, page->ebox->tag);

	return 0;
}

/*--------------------------------------------------
traverse the page list and print it.
return:
	0	OK
	<0	fails
---------------------------------------------------*/
int egi_page_travlist(EGI_PAGE *page)
{
	struct list_head *tnode;
	EGI_EBOX *ebox;

	/* check data */
	if(page==NULL)
	{
		printf("egi_page_travlist(): input egi_page *page is NULL!\n");
		return -1;
	}
	/* check list */
	if(list_empty(&page->list_head))
	{
		printf("egi_page_travlist(): page '%s' has an empty list_head.\n",page->ebox->tag);
		return -2;
	}

	/* traverse the list, not safe */
	list_for_each(tnode, &page->list_head)
	{
		ebox=list_entry(tnode, EGI_EBOX, node);
		printf("egi_page_travlist(): find child --- ebox: '%s' --- \n",ebox->tag);
	}


	return 0;
}



/*--------------------------------------------------
activate a page and its eboxes in its list.

return:
	0	OK
	<0	fails
---------------------------------------------------*/
int egi_page_activate(EGI_PAGE *page)
{
	struct list_head *tnode;
	EGI_EBOX *ebox;
	int ret=0;

	/* check data */
	if(page==NULL)
	{
		printf("egi_page_activate(): input egi_page *page is NULL!\n");
		return -1;
	}
	/* check list */
	if(list_empty(&page->list_head))
	{
		printf("egi_page_activate(): page '%s' has an empty list_head .\n",page->ebox->tag);
		return -2;
	}

	/* set page status */
	page->ebox->status=status_active;

        /* !!!!! in page->ebox.refresh(), the page->fpath will NOT be seen and handled, it's a page method.
	load a picture or use prime color as wallpaper*/
        if(page->fpath != NULL)
                show_jpg(page->fpath, &gv_fb_dev, SHOW_BLACK_NOTRANSP, 0, 0);
        else /* use ebox prime color to clear(fill) screen */
        {
                if(page->ebox->prmcolor >= 0)
                         clear_screen(&gv_fb_dev, page->ebox->prmcolor);
        }


	/* traverse the list and activate list eboxes, not safe */
	list_for_each(tnode, &page->list_head)
	{
		ebox=list_entry(tnode, EGI_EBOX, node);
		ret=ebox->activate(ebox);
		egi_pdebug(DBG_PAGE,"egi_page_activate(): activate page list item ebox: '%s' with ret=%d \n",ebox->tag,ret);
	}


	return 0;
}


/*--------------------------------------------------
refresh a page and its eboxes in its list.

return:
	1	need_refresh=false
	0	OK
	<0	fails
---------------------------------------------------*/
int egi_page_refresh(EGI_PAGE *page)
{
	struct list_head *tnode;
	EGI_EBOX *ebox;
	int ret;

	/* check data */
	if(page==NULL || page->ebox==NULL )
	{
		printf("egi_page_refresh(): input egi_page * page or page->ebox is NULL!\n");
		return -1;
	}

	/* --------------- ***** FOR PAGE REFRESH ***** ------------ */
	/* only if need_refresh */
	if(page->ebox->need_refresh)
	{
		/* load a picture or use prime color as wallpaper*/
		if(page->fpath != NULL)
			show_jpg(page->fpath, &gv_fb_dev, SHOW_BLACK_NOTRANSP, 0, 0);
		else /* use ebox prime color to clear(fill) screen */
		{
			if(page->ebox->prmcolor >= 0)
				 clear_screen(&gv_fb_dev, page->ebox->prmcolor);
		}

		/* reset need_refresh */
		page->ebox->need_refresh=false;
	}

	/* --------------- ***** FOR PAGE CHILD REFRESH ***** ------------*/
	/* check list */
	if(list_empty(&page->list_head))
	{
		printf("egi_page_refresh(): page '%s' has an empty list_head .\n",page->ebox->tag);
		return -2;
	}

	/* traverse the list and activate list eboxes, not safe */
	list_for_each(tnode, &page->list_head)
	{
		ebox=list_entry(tnode, EGI_EBOX, node);
		ret=ebox->refresh(ebox);
		egi_pdebug(DBG_PAGE,"egi_page_refresh(): refresh page list item ebox: '%s' with ret=%d \n",ebox->tag,ret);
	}

	/* reset need_refresh */
	page->ebox->need_refresh=false;


	return 0;
}


/*--------------------------------------
default page routine job

return:
	loop or >=0  	OK
	<0		fails
----------------------------------------*/
int egi_page_routine(EGI_PAGE *page)
{
	int i,j;
	int ret;
	uint16_t sx,sy;
	EGI_EBOX  *hitbtn; /* hit button_ebox */

	/* 1. check data */
	if(page==NULL)
	{
		printf("egi_page_routine(): input egi_page *page is NULL!\n");
		return -1;
	}

	/* 2. check list */
	if(list_empty(&page->list_head))
	{
		printf("egi_page_routine(): WARNING!!! page '%s' has an empty ebox list_head .\n",page->ebox->tag);
	}


	/* 3. load threads */
	for(i=0;i<EGI_PAGE_MAXTHREADS;i++)
	{
		if( page->runner[i] !=0 )
		{
			if( pthread_create(&page->threadID[i],NULL,page->runner[i],(void *)page)==0)
			{
				page->thread_running[i]=true;
				printf("egi_page_routine(): create pthreadID[%d]=%u successfully. \n",
								i, (unsigned int)page->threadID[i]);
			}
			else
				printf("egi_page_routine(): fail to create pthread ID=%d \n",i);
		}
	}

	/* 4. loop in touch checking and other routines.... */
	while(1)
	{
		/* 4.1. necessary wait for XPT */
	 	tm_delayms(2);

		/* 4.2. read XPT to get avg tft-LCD coordinate */
                //printf("start xpt_getavt_xy() \n");
                ret=xpt_getavg_xy(&sx,&sy); /* if fail to get touched tft-LCD xy */

		/* 4.3. touch reading is going on... */
                if(ret == XPT_READ_STATUS_GOING )
                {
                        //printf("XPT READ STATUS GOING ON....\n");
                        continue; /* continue to loop to finish reading touch data */
                }

                /* 4.4. put PEN-UP status events here */
                else if(ret == XPT_READ_STATUS_PENUP )
                {
                        //eig_pdebug(DBG_PAGE,"egi_page_routine(): --- XPT_READ_STATUS_PENUP ---\n");
			//egi_page_refresh(page);
			tm_delayms(100);/* hold on for a while, or the screen will be  */

			/* load cpuload motion icons */
#if 0
			i++;
			if(i>3)i=0;
                        symbol_motion_string(&gv_fb_dev, 155-i*30, &sympg_icons,
      		                               	       1, 150,0, &symmic_cpuload[i][0]);
#endif

		}

		/* 4.5. get touch coordinates and trigger actions for hit button if any */
                else if(ret == XPT_READ_STATUS_COMPLETE)
                {
                        //eig_pdebug(DBG_PAGE,"egi_page_routine(): --- XPT_READ_STATUS_COMPLETE ---\n");

	 /* ----------------    Touch Event Handling   ----------------  */

	                hitbtn=egi_hit_pagebox(sx, sy, page);
	      	        if(hitbtn != NULL)
				egi_pdebug(DBG_PAGE,"egi_page_routine(): button '%s' of page '%s' is touched!\n",
										hitbtn->tag,page->ebox->tag);
	                continue;

			/* loop in refreshing listed eboxes */

		}

	}

	return 0;
}
