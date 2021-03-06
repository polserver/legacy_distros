// History
//   2005/03/02 Shinigami: internal_MoveItem - item took from container don't had the correct realm
//   2005/04/02 Shinigami: mf_CreateItemCopyAtLocation - added realm param
//   2005/04/28 Shinigami: mf_EnumerateItemsInContainer/enumerate_contents - added flag to list content of locked container too
//   2005/04/28 Shinigami: added mf_SecureTradeWin - to init secure trade via script over long distances
//                         added mf_MoveItemToSecureTradeWin - to move item to secure trade window via script
//   2005/04/31 Shinigami: mf_EnumerateItemsInContainer - error message added
//   2005/06/01 Shinigami: added mf_Attach - to attach a Script to a Character
//                         mf_MoveItem - stupid non-realm anti-crash bugfix (e.g. for intrinsic weapons without realm)
//                         added mf_ListStaticsAtLocation - list static Items at location
//                         added mf_ListStaticsNearLocation - list static Items around location
//                         added mf_GetStandingLayers - get layer a mobile can stand
//   2005/06/06 Shinigami: mf_ListStatics* will return multi Items too / flags added
//   2005/07/04 Shinigami: mf_ListStatics* constants renamed
//                         added Z to mf_ListStatics*Location - better specify what u want to get
//                         modified Z handling of mf_ListItems*Location* and mf_ListMobilesNearLocation*
//                           better specify what u want to get
//                         added realm-based coord check to mf_ListItems*Location*,
//                           mf_List*InBox and mf_ListMobilesNearLocation*
//                         added mf_ListStaticsInBox - list static items in box
//   2005/07/07 Shinigami: realm-based coord check in mf_List*InBox disabled
//   2005/09/02 Shinigami: mf_Attach - smaller bug which allowed u to attach more than one Script to a Character
//   2005/09/03 Shinigami: mf_FindPath - added Flags to support non-blocking doors
//   2005/09/23 Shinigami: added mf_SendStatus - to send full status packet to support extensions
//   2005/12/06 MuadDib: Rewrote realm handling for Move* and internal_move_item. New realm is
//                       to be passed to function, and oldrealm handling is done within the function.
//                       Container's still save and check oldrealm as before to update clients.
//   2006/01/18 Shinigami: added attached_npc_ - to get attached NPC from AI-Script-Process Obj
//   2006/05/10 Shinigami: mf_ListMobilesNearLocationEx - just a logical mismatch
//   2006/05/24 Shinigami: added mf_SendCharacterRaceChanger - change Hair, Beard and Color
//   2006/06/08 Shinigami: started to add FindPath logging
//   2006/09/17 Shinigami: mf_SendEvent() will return error "Event queue is full, discarding event"
//   2006/11/25 Shinigami: mf_GetWorldHeight() fixed bug because z was'n initialized
//   2007/05/03 Turley:    added mf_GetRegionNameAtLocation - get name of justice region
//   2007/05/04 Shinigami: added mf_GetRegionName - get name of justice region by objref
//   2007/05/07 Shinigami: fixed a crash in mf_GetRegionName using NPCs; added TopLevelOwner

#include "clib/stl_inc.h"

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif


#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "clib/cfgelem.h"
#include "clib/cfgfile.h"
#include "clib/clib.h"
#include "clib/endian.h"
#include "clib/esignal.h"
#include "clib/logfile.h"
#include "clib/mlog.h"
#include "clib/passert.h"
#include "clib/weakptr.h"
#include "clib/stlutil.h"
#include "clib/strutil.h"

#include "bscript/berror.h"
#include "bscript/bobject.h"
#include "bscript/execmodl.h"
#include "bscript/impstr.h"
#include "bscript/token.h"

#include "plib/mapcell.h"
#include "plib/mapshape.h"
#include "plib/maptile.h"
#include "plib/realm.h"

#include "action.h"
#include "boat.h"
#include "cfgemod.h"
#include "cfgrepos.h"
#include "cgdata.h"
#include "charactr.h"
#include "client.h"
#include "core.h"
#include "eventid.h"
#include "fnsearch.h"
#include "guardrgn.h"
#include "itemdesc.h"
#include "listenpt.h"
#include "los.h"
#include "menu.h"
#include "multidef.h"
#include "nmsgtype.h"
#include "npc.h"
#include "objtype.h"
#include "osemod.h"
#include "polcfg.h"
#include "polclass.h"
#include "polsig.h"
#include "poltype.h"
#include "profile.h"
#include "realms.h"
#include "savedata.h"
#include "scrsched.h"
#include "scrstore.h"
#include "skilladv.h"
#include "spells.h"
#include "ssopt.h"
#include "target.h"
#include "ufunc.h"
#include "umanip.h"
#include "udatfile.h"
#include "uofile.h"
#include "uoscrobj.h"
#include "ustruct.h"
#include "uvars.h"
#include "uworld.h"
#include "uoexec.h"
#include "uopathnode.h"

#include "uoemod.h"

#include "objecthash.h"

#define CONST_DEFAULT_ZRANGE 19

class EMenuObjImp : public BApplicObj< Menu >
{
public:
    EMenuObjImp( const Menu& m ) : BApplicObj<Menu>(&menu_type, m) {}
    virtual const char* typeOf() const { return "MenuRef"; }
    virtual BObjectImp* copy() const { return new EMenuObjImp(value()); }
};


UOExecutorModule::UOExecutorModule(UOExecutor& exec) :
    ExecutorModule("UO", exec),
    uoexec( exec ),
    target_cursor_chr(NULL),
    menu_selection_chr(NULL),
    prompt_chr(NULL),
    gump_chr(NULL),
    textentry_chr(NULL),
    resurrect_chr(NULL),
    selcolor_chr(NULL),
    target_options(0),
    attached_chr_(NULL),
    attached_npc_(NULL),
    controller_(NULL),
    reserved_items_(),
    registered_for_speech_events(false)
{
}

UOExecutorModule::~UOExecutorModule()
{
    while (!reserved_items_.empty())
    {
        Item* item = reserved_items_.back().get();
        item->inuse( false );
        reserved_items_.pop_back();
    }

    if (target_cursor_chr != NULL)
    {
        // CHECKME can we cancel the cursor request?
        target_cursor_chr->client->gd->target_cursor_uoemod = NULL;
        target_cursor_chr = NULL;
    }
    if (menu_selection_chr != NULL)
    {
        menu_selection_chr->client->gd->menu_selection_uoemod = NULL;
        menu_selection_chr = NULL;
    }
    if (prompt_chr != NULL)
    {
        prompt_chr->client->gd->prompt_uoemod = NULL;
        prompt_chr = NULL;
    }
    if (gump_chr != NULL)
    {
        gump_chr->client->gd->remove_gumpmod( this );
        // gump_chr->client->gd->gump_uoemod = NULL;
        gump_chr = NULL;
    }
    if (textentry_chr != NULL)
    {
        textentry_chr->client->gd->textentry_uoemod = NULL;
        textentry_chr = NULL;
    }
    if (resurrect_chr != NULL)
    {
        resurrect_chr->client->gd->resurrect_uoemod = NULL;
        resurrect_chr = NULL;
    }
    if (selcolor_chr != NULL)
    {
        selcolor_chr->client->gd->selcolor_uoemod = NULL;
        selcolor_chr = NULL;
    }
    if (attached_chr_ != NULL)
    {
		passert( attached_chr_->script_ex == &uoexec );
        attached_chr_->script_ex = NULL;
        attached_chr_ = NULL;
    }
    if (registered_for_speech_events)
    {
        deregister_from_speech_events( &uoexec );
    }
}

BObjectImp* UOExecutorModule::mf_Attach(/* Character */)
{
    Character* chr;
    if (getCharacterParam( exec, 0, chr ))
    {
		if (attached_chr_ == NULL)
        {
            if (chr->script_ex == NULL)
			{
				attached_chr_ = chr;
				attached_chr_->script_ex = &uoexec;

				return new BLong(1);
			}
			else
				return new BError( "Another script still attached." );
        }
        else
            return new BError( "Another character still attached." );
    }
    else
        return new BError( "Invalid parameter" );
}

BObjectImp* UOExecutorModule::mf_Detach()
{
    if (attached_chr_ != NULL)
    {
        passert( attached_chr_->script_ex == &uoexec );
        attached_chr_->script_ex = NULL;
        attached_chr_ = NULL;
        return new BLong(1);
    }
    else
    {
        return new BLong(0);
    }
}

static bool item_create_params_ok( long objtype, long amount )
{
    return ((objtype >= UOBJ_ITEM__LOWEST && objtype <= UOBJ_ITEM__HIGHEST) ||
            (objtype >= EXTOBJ__LOWEST && objtype <= EXTOBJ__HIGHEST)) &&
           amount > 0 &&
           amount <= 60000L;
}

BObjectImp* _create_item_in_container(
                                       UContainer* cont,
                                       const ItemDesc* descriptor,
                                       unsigned short amount,
                                       bool force_stacking,
                                       UOExecutorModule* uoemod )
{
    if ((tile_flags( descriptor->graphic ) & FLAG::STACKABLE) || force_stacking)
    {
        for( UContainer::const_iterator itr = cont->begin(); itr != cont->end(); ++itr )
	    {
		    Item *item = GET_ITEM_PTR( itr );
			ItemRef itemref(item); //dave 1/28/3 prevent item from being destroyed before function ends

			if (item->objtype_ == descriptor->objtype &&
                !item->newbie() &&
				item->color == descriptor->color && //dave added 5/11/3, only add to existing stack if is default color
				item->has_only_default_cprops(descriptor) &&       //dave added 5/11/3, only add to existing stack if default cprops
                (!item->inuse() || (uoemod && uoemod->is_reserved_to_me(item))) &&
                item->can_add_to_self(amount))
            {

				//DAVE added this 11/17, call can/onInsert scripts for this container
				Character* chr_owner = cont->GetCharacterOwner();
				if(chr_owner == NULL)
					chr_owner = uoemod->controller_.get();

                //If the can insert script fails for combining a stack, we'll let the create new item code below handle it
                // what if a cannInsert script modifies (adds to) the container we're iterating over? (they shouldn't do that)
                // FIXME oh my, this makes no sense.  'item' in this case is already in the container.
                if(!cont->can_insert_increase_stack( chr_owner, UContainer::MT_CORE_CREATED, item, amount, NULL ))
					continue;
				if(item->orphan()) //dave added 1/28/3, item might be destroyed in RTC script
				{
					return new BError( "Item was destroyed in CanInsert Script" );
				}
                #ifdef PERGON
                item->ct_merge_stacks_pergon( amount ); // Pergon: Re-Calculate Property CreateTime after Adding Items to a Stack
                #endif

                long newamount = item->getamount();
                newamount += amount;
                item->setamount( static_cast<unsigned short>(newamount) );

                update_item_to_inrange( item );
                UpdateCharacterWeight( item );

                // FIXME again, this makes no sense, item is already in the container.
                cont->on_insert_increase_stack( chr_owner, UContainer::MT_CORE_CREATED, item, amount );
    			if(item->orphan()) //dave added 1/28/3, item might be destroyed in RTC script
				{
					return new BError( "Item was destroyed in OnInsert Script" );
				}


                return new EItemRefObjImp(item);
            }
	    }

/*
        long maxstack = 60000L - amount;
        if (maxstack > 0)
        {
            Item* found = cont->find_toplevel_objtype( objtype, maxstack );
            if (found != NULL && !found->newbie())
            {
                if (!found->inuse() || (uoemod && uoemod->is_reserved_to_me(found)) )
                {
                    // weight-limit check
                    long newamount = found->getamount();
                    newamount += amount;
                    if (newamount <= 60000) // this test is probably unnecessary.
                    {
                        found->setamount( newamount );
                        update_item_to_inrange( found );
                        return new EItemRefObjImp( found );
                    }
                }
            }
        }
*/
    }
    else if (amount != 1 && !force_stacking)
    {
        return new BError( "That item is not stackable.  Create one at a time." );
    }

    Item* item = Item::create( *descriptor );
    if (item != NULL)
    {
		ItemRef itemref(item); //dave 1/28/3 prevent item from being destroyed before function ends
		item->realm = cont->realm;
        item->setamount( amount );

        if (cont->can_add( *item ))
        {
            if (!descriptor->create_script.empty())
            {
                BObjectImp* res = run_script_to_completion( descriptor->create_script, new EItemRefObjImp( item ) );
                if (!res->isTrue())
                {
                    item->destroy();
                    return res;
                }
                else
                {
                    BObject ob( res );
                }
                if (!cont->can_add( *item ))
                {                               // UNTESTED
                    item->destroy();
                    return new BError( "Couldn't add item to container after create script ran" );
                }
            }

			//DAVE added this 11/17, call can/onInsert scripts for this container
			Character* chr_owner = cont->GetCharacterOwner();
			if(chr_owner == NULL)
				chr_owner = uoemod->controller_.get();

			if(!cont->can_insert_add_item( chr_owner, UContainer::MT_CORE_CREATED, item ))
			{
				item->destroy();
				//FIXME: try to propogate error from returned result of the canInsert script
				return new BError( "Could not insert item into container." );
			}
			if(item->orphan()) //dave added 1/28/3, item might be destroyed in RTC script
			{
				return new BError( "Item was destroyed in CanInsert Script" );
			}

            cont->add_at_random_location( item );
            update_item_to_inrange( item );
            //DAVE added this 11/17, refresh owner's weight on item insert
			UpdateCharacterWeight( item );

			cont->on_insert_add_item( chr_owner, UContainer::MT_CORE_CREATED, item );
			if(item->orphan()) //dave added 1/28/3, item might be destroyed in RTC script
			{
				return new BError( "Item was destroyed in OnInsert Script" );
			}

            return new EItemRefObjImp( item );
        }
        else
        {
            item->destroy();
            return new BError( "That container is full" );
        }
    }
    else
    {
        return new BError( "Failed to create that item type" );
    }
}

BObjectImp* UOExecutorModule::mf_CreateItemInContainer()
{
    Item* item;
    const ItemDesc* descriptor;
    long amount;

    if (getItemParam( exec, 0, item ) &&
        getObjtypeParam( exec, 1, descriptor ) &&
        getParam( 2, amount ) &&
        item_create_params_ok( descriptor->objtype, amount ))
    {
        if (item->isa( UObject::CLASS_CONTAINER ))
        {
            return _create_item_in_container( static_cast<UContainer*>(item),
                                                descriptor,
                                                static_cast<unsigned short>(amount),
                                                false,
                                                this );
        }
        else
        {
            return new BError( "That is not a container" );
        }
    }
    else
    {
        return new BError( "A parameter was invalid" );
    }
}

BObjectImp* UOExecutorModule::mf_CreateItemInInventory()
{
    Item* item;
    const ItemDesc* descriptor;
    long amount;

    if (getItemParam( exec, 0, item ) &&
        getObjtypeParam( exec, 1, descriptor ) &&
        getParam( 2, amount ) &&
        item_create_params_ok( descriptor->objtype, amount ))
    {
        if (item->isa( UObject::CLASS_CONTAINER ))
        {
            return _create_item_in_container( static_cast<UContainer*>(item),
                                              descriptor,
                                              static_cast<unsigned short>(amount),
                                              true,
                                              this );
        }
        else
        {
            return new BError( "That is not a container" );
        }
    }
    else
    {
        return new BError( "A parameter was invalid" );
    }
}


BObjectImp* UOExecutorModule::broadcast()
{
	const char *text;
    unsigned short font;
    unsigned short color;
	text = exec.paramAsString(0);
	if (text &&
        getParam( 1, font ) && // todo: getFontParam
        getParam( 2, color ))   // todo: getColorParam
    {
        ::broadcast( text, font, color );
        return new BLong( 1 );
    }
    else
    {
        return NULL;
    }
}

/* Special containers (specifically, bankboxes, but probably any other
   "invisible" accessible container) seem to work thus:
   They sit in layer 0x1D.  The player is told to "wear_item" the item,
   then the gump and container contents are sent.
   We'll put a reference to this item in the character's additional_legal_items
   container, which is flushed whenever he moves.
   It might be better to actually put it in that layer, because that way
   it implicitly is at the same location (location of the wornitems container)
*/
BObjectImp* UOExecutorModule::mf_SendOpenSpecialContainer()
{
    Character* chr;
    Item* item;
    if (!getCharacterParam( exec, 0, chr ) ||
        !getItemParam( exec, 1, item ))
    {
        return new BError( "Invalid parameter type" );
    }

    if (!chr->has_active_client())
    {
        return new BError( "No client attached" );
    }
    if (!item->isa( UObject::CLASS_CONTAINER ))
    {
        return new BError( "That isn't a container" );
    }

    u8 save_layer = item->layer;
    item->layer = LAYER_BANKBOX;
    send_wornitem( chr->client, chr, item );
    item->layer = save_layer;
    item->x = chr->x;
    item->y = chr->y;
    item->z = chr->z;
    item->double_click( chr->client );              // open the container on the client's screen
    chr->add_remote_container( item );

    return new BLong(1);
}

BObjectImp* open_trade_window( Client* client, Character* dropon );
BObjectImp* UOExecutorModule::mf_SecureTradeWin()
{
    Character* chr;
    Character* chr2;
    if (!getCharacterParam( exec, 0, chr ) ||
        !getCharacterParam( exec, 1, chr2 ))
    {
        return new BError( "Invalid parameter type." );
    }

    if (chr == chr2)
    {
        return new BError( "You can't trade with yourself." );
    }

    if (!chr->has_active_client())
    {
        return new BError( "No client attached." );
    }
    if (!chr2->has_active_client())
    {
        return new BError( "No client attached." );
    }
    
    return open_trade_window( chr->client, chr2 );
}

void cancel_trade( Character* chr1 );
BObjectImp* UOExecutorModule::mf_CloseTradeWindow()
{
	Character* chr;
	if ( !getCharacterParam( exec, 0, chr ) )
	{
		return new BError( "Invalid parameter type." );
	}
	if ( !chr->trading_with )
		return new BError("Mobile is not currently trading with anyone.");
    
	cancel_trade(chr);
	return new BLong(1);
}

BObjectImp* UOExecutorModule::mf_SendViewContainer()
{
    Character* chr;
    Item* item;
    if (!getCharacterParam( exec, 0, chr ) ||
        !getItemParam( exec, 1, item ))
    {
        return new BError( "Invalid parameter type" );
    }

    if (!chr->has_active_client())
    {
        return new BError( "No client attached" );
    }
    if (!item->isa( UObject::CLASS_CONTAINER ))
    {
        return new BError( "That isn't a container" );
    }
    UContainer* cont = static_cast<UContainer*>(item);
    chr->client->pause();
    send_open_gump( chr->client, *cont );
    send_container_contents( chr->client, *cont );
    chr->client->restart();
    return new BLong(1);
}

BObjectImp* UOExecutorModule::mf_FindObjtypeInContainer()
{
    Item* item;
    unsigned short objtype;
    if (!getItemParam( exec, 0, item ) ||
        !getObjtypeParam( exec, 1, objtype ))
    {
        return new BError( "Invalid parameter type" );
    }
    if (!item->isa( UObject::CLASS_CONTAINER ))
    {
        return new BError( "That is not a container" );
    }

	UContainer* cont = static_cast<UContainer*>(item);
	Item* found = cont->find_toplevel_objtype( objtype );
	if (found == NULL)
		return new BError( "No items were found" );
	else
        return new EItemRefObjImp( found );
}


BObjectImp* UOExecutorModule::mf_SendSysMessage()
{
    Character* chr;
    const String* ptext;
    unsigned short font;
    unsigned short color;

    if (getCharacterParam( exec, 0, chr ) &&
        ( (ptext = getStringParam( 1 )) != NULL) &&
        getParam( 2, font ) &&
        getParam( 3, color ))
    {
        if (chr->has_active_client())
        {
            send_sysmessage( chr->client, ptext->data(), font, color );
            return new BLong(1);
        }
        else
        {
            return new BError( "Mobile has no active client" );
        }
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* UOExecutorModule::mf_PrintTextAbove()
{
    UObject* obj;
    const String* ptext;
    unsigned short font;
    unsigned short color;

    if (getUObjectParam( exec, 0, obj ) &&
        getStringParam( 1, ptext ) &&
        getParam( 2, font ) &&
        getParam( 3, color ))
    {
        return new BLong( say_above( obj, ptext->data(), font, color ) );
    }
    else
    {
        return new BError( "A parameter was invalid" );
    }
}
BObjectImp* UOExecutorModule::mf_PrivateTextAbove()
{
    Character* chr;
    UObject* obj;
    const String* ptext;
    unsigned short font;
    unsigned short color;

    if (getUObjectParam( exec, 0, obj ) &&
        getStringParam( 1, ptext ) &&
        getCharacterParam( exec, 2, chr ) &&
        getParam( 3, font ) &&
        getParam( 4, color ) )
    {
        return new BLong( private_say_above( chr, obj, ptext->data(), font, color ) );
    }
    else
    {
        return new BError( "A parameter was invalid" );
    }
}

const long TGTOPT_CHECK_LOS     = 0x0001;
const long TGTOPT_NOCHECK_LOS   = 0x0000;
const long TGTOPT_HARMFUL       = 0x0002;
const long TGTOPT_HELPFUL       = 0x0004;

// FIXME susceptible to out-of-sequence target cursors
void handle_script_cursor( Character* chr, UObject* obj )
{
    if (chr != NULL &&
        chr->client->gd->target_cursor_uoemod != NULL)
    {
        if (obj != NULL)
        {
            if (obj->ismobile())
            {
                Character* targetted_chr = static_cast<Character*>(obj);
                if (chr->client->gd->target_cursor_uoemod->target_options & TGTOPT_HARMFUL)
                {
                    targetted_chr->inform_engaged( chr );
                    chr->repsys_on_attack( targetted_chr );
                }
                else if (chr->client->gd->target_cursor_uoemod->target_options & TGTOPT_HELPFUL)
                {
                    chr->repsys_on_help( targetted_chr );
                }
            }
            chr->client->gd->target_cursor_uoemod->uoexec.ValueStack.top().set( new BObject( obj->make_ref() ) );
        }
        // even on cancel, we wake the script up.
        chr->client->gd->target_cursor_uoemod->uoexec.os_module->revive();
        chr->client->gd->target_cursor_uoemod->target_cursor_chr = NULL;
        chr->client->gd->target_cursor_uoemod = NULL;
    }
}

LosCheckedTargetCursor los_checked_script_cursor( &handle_script_cursor, true );
NoLosCheckedTargetCursor nolos_checked_script_cursor( &handle_script_cursor, true );


BObjectImp* UOExecutorModule::mf_Target()
{
    Character* chr;
    if (getCharacterParam( exec, 0, chr ))
    {
        if (chr->has_active_client())
        {
            if (!chr->target_cursor_busy())
            {
                if (!getParam( 1, target_options ))
                    target_options = TGTOPT_CHECK_LOS;

                MSG6C_TARGET_CURSOR::CURSOR_TYPE crstype;

                if (target_options & TGTOPT_HARMFUL)
                    crstype = MSG6C_TARGET_CURSOR::CURSOR_TYPE_HARMFUL;
                else if (target_options & TGTOPT_HELPFUL)
                    crstype = MSG6C_TARGET_CURSOR::CURSOR_TYPE_HELPFUL;
                else
                    crstype = MSG6C_TARGET_CURSOR::CURSOR_TYPE_NEUTRAL;

                if ((target_options & TGTOPT_CHECK_LOS) && !chr->ignores_line_of_sight())
                    if (los_checked_script_cursor.send_object_cursor( chr->client, crstype ) )
					{
			            chr->client->gd->target_cursor_uoemod = this;
				        target_cursor_chr = chr;
		                uoexec.os_module->suspend();
					    return new BLong(0);
					}
					else
					{
						return new BError( "Client has an active target cursor" );
					}
				else
				{
					if ( nolos_checked_script_cursor.send_object_cursor( chr->client, crstype ) )
					{
			            chr->client->gd->target_cursor_uoemod = this;
				        target_cursor_chr = chr;
		                uoexec.os_module->suspend();
				        return new BLong(0);
					}
					else
					{
						return new BError( "Client has an active target cursor" );
					}
				}
				return new BLong(0);
            }
            else
            {
                return new BError( "Client busy with another target cursor" );
            }
        }
        else
        {
            return new BError( "No client connected" );
        }
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* UOExecutorModule::mf_TargetCancel()
{
    Character* chr;
    if (getCharacterParam( exec, 0, chr ))
    {
        if (chr->has_active_client())
        {
            if (chr->target_cursor_busy())
            {
					static MSG6C_TARGET_CURSOR msg;
					msg.msgtype = MSG6C_TARGET_CURSOR_ID;
					msg.unk1 = MSG6C_TARGET_CURSOR::UNK1_00;
					msg.target_cursor_serial = ctBEu32( 0x0 );
					msg.cursor_type = 0x3; 
					chr->client->transmit( &msg, sizeof msg );
					return new BLong(0);
            }
            else
            {
                return new BError( "Client does not have an active target cursor" );
            }
        }
        else
        {
            return new BError( "No client connected" );
        }
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

void handle_coord_cursor( Character* chr, MSG6C_TARGET_CURSOR* msg )
{
    if (chr != NULL &&
        chr->client->gd->target_cursor_uoemod != NULL)
    {
        if (msg != NULL)
        {
            BStruct* arr = new BStruct;
            arr->addMember( "x", new BLong( cfBEu16(msg->x) ) );
            arr->addMember( "y", new BLong( cfBEu16(msg->y) ) );
            arr->addMember( "z", new BLong( msg->z ) );
//			FIXME: Objtype CANNOT be trusted! Scripts must validate this, or, we must
//			validate right here. Should we check map/static, or let that reside
//			for scripts to run ListStatics? In theory, using Injection or similar,
//			you could mine where no mineable tiles are by faking objtype in packet
//			and still get the resources?
			arr->addMember( "objtype", new BLong( cfBEu16(msg->graphic) ) );

            u32 selected_serial = cfBEu32(msg->selected_serial);
            if (selected_serial)
            {
                UObject* obj = system_find_object( selected_serial );
                if (obj)
                {
                    arr->addMember( obj->target_tag(), obj->make_ref() );
                }
            }

//			Never trust packet's objtype is the reason here. They should never
//			target on other realms. DUH!
			arr->addMember( "realm", new String( chr->realm->name() ) );

            chr->client->gd->target_cursor_uoemod->uoexec.ValueStack.top().set( new BObject( arr ) );

        }

        chr->client->gd->target_cursor_uoemod->uoexec.os_module->revive();
        chr->client->gd->target_cursor_uoemod->target_cursor_chr = NULL;
        chr->client->gd->target_cursor_uoemod = NULL;
    }
}

LosCheckedCoordCursor script_cursor2( &handle_coord_cursor, true );
BObjectImp* UOExecutorModule::mf_TargetCoordinates()
{
    Character* chr;
    if (getCharacterParam( exec, 0, chr ))
    {
        if (chr->has_active_client())
        {
            if (!chr->target_cursor_busy())
            {
                if (script_cursor2.send_coord_cursor( chr->client ))
				{
					chr->client->gd->target_cursor_uoemod = this;
		            target_cursor_chr = chr;
			        uoexec.os_module->suspend();
				    return new BLong(0);
				}
				else
				{
					return new BError( "Client has an active target cursor" );
				}

			}
            else
            {
                return new BError( "Client has an active target cursor" );
            }
        }
        else
        {
            return new BError( "Mobile has no active client" );
        }
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

MultiPlacementCursor multi_placement_cursor( &handle_coord_cursor );
BObjectImp* UOExecutorModule::mf_TargetMultiPlacement()
{
    Character* chr;
    unsigned short objtype;
    long flags;
	long xoffset, yoffset;
    if (! (getCharacterParam( exec, 0, chr ) &&
           getObjtypeParam( exec, 1, objtype ) &&
           getParam(2, flags) &&
		   getParam(3, xoffset) &&
		   getParam(4, yoffset)) )
    {
        return new BError( "Invalid parameter type" );
    }


    if (!chr->has_active_client())
    {
        return new BError( "No client attached" );
    }

    if (chr->target_cursor_busy())
    {
        return new BError( "Client busy with another target cursor" );
    }

    if (find_itemdesc(objtype).type != ItemDesc::BOATDESC &&
        find_itemdesc(objtype).type != ItemDesc::HOUSEDESC)
    {
        return new BError( "Object Type is out of range for Multis" );
    }

    chr->client->gd->target_cursor_uoemod = this;
    target_cursor_chr = chr;
    multi_placement_cursor.send_placemulti( chr->client, objtype, flags, (s16)xoffset, (s16)yoffset );
    uoexec.os_module->suspend();
    return new BLong(0);
}

BObjectImp* UOExecutorModule::mf_GetObjType()
{
    Item* item;
    Character* chr;

    if (getItemParam( exec, 0, item ))
    {
        return new BLong( item->objtype_ );
    }
    else if (getCharacterParam( exec, 0, chr ))
    {
        return new BLong( chr->objtype_ );
    }
    else
    {
        return new BLong(0);
    }
}

// FIXME character needs an Accessible that takes an Item*
BObjectImp* UOExecutorModule::mf_Accessible()
{
    Character* chr;
    Item* item;
    if (getCharacterParam( exec, 0, chr ) &&
        getItemParam( exec, 1, item ))
    {
        if (find_legal_item( chr, item->serial ) != NULL)
            return new BLong(1);
        else
            return new BLong(0);
    }
    return new BLong(0);
}

BObjectImp* UOExecutorModule::mf_GetAmount()
{
    Item* item;

    if (getItemParam( exec, 0, item ))
    {
        return new BLong(item->getamount());
    }
    else
    {
        return new BLong(0);
    }
}


// FIXME, copy-pasted from NPC, copy-pasted to CreateItemInContainer
BObjectImp* UOExecutorModule::mf_CreateItemInBackpack()
{
    Character* chr;
    const ItemDesc* descriptor;
    unsigned short amount;

    if (getCharacterParam( exec, 0, chr ) &&
        getObjtypeParam( exec, 1, descriptor ) &&
        getParam( 2, amount ) &&
        item_create_params_ok( descriptor->objtype, amount ))
    {
        UContainer* backpack = chr->backpack();
        if (backpack != NULL)
        {
            return _create_item_in_container( backpack, descriptor, amount, false, this );
        }
        else
        {
            return new BError( "Character has no backpack." );
        }
    }
    else
    {
        return new BError( "A parameter was invalid." );
    }
}

BObjectImp* _complete_create_item_at_location( Item* item, long x, long y, long z, Realm* realm )
{
	//Dave moved these 3 lines up here 12/20 cuz x,y,z was uninit in the createscript.
    item->x = static_cast<unsigned short>(x);
    item->y = static_cast<unsigned short>(y);
    item->z = static_cast<signed char>(z);
	item->realm = realm;

    // ITEMDESCTODO: use the original descriptor
    const ItemDesc& id = find_itemdesc( item->objtype_ );
    if (!id.create_script.empty())
    {
        BObjectImp* res = run_script_to_completion( id.create_script, new EItemRefObjImp( item ) );
        if (!res->isTrue())
        {
            item->destroy();
            return res;
        }
        else
        {
            BObject ob( res );
        }
    }

    update_item_to_inrange( item );
    add_item_to_world( item );
    register_with_supporting_multi( item );
    return new EItemRefObjImp( item );
}

BObjectImp* UOExecutorModule::mf_CreateItemAtLocation(/* x,y,z,objtype,amount,realm */)
{
    long x, y, z;
    const ItemDesc* itemdesc;
    unsigned short amount;
	const String* strrealm;
    if (getParam( 0, x ) &&
        getParam( 1, y ) &&
        getParam( 2, z, ZCOORD_MIN, ZCOORD_MAX ) &&
        getObjtypeParam( exec, 3, itemdesc ) &&
        getParam( 4, amount, 1, 60000 ) &&
		getStringParam( 5, strrealm ) &&
        item_create_params_ok( itemdesc->objtype, amount ))
    {
        if (!(tile_flags( itemdesc->graphic ) & FLAG::STACKABLE) &&
             (amount != 1))
        {
            return new BError( "That item is not stackable.  Create one at a time." );
        }

		Realm* realm = find_realm(strrealm->value());
		if(!realm)
			return new BError( "Realm not found" );

		if(!realm->valid(x,y,z)) return new BError("Invalid Coordinates for Realm");
        Item* item = Item::create( *itemdesc );
        if (item != NULL)
        {
            item->setamount( amount );
            return _complete_create_item_at_location( item, x, y, z, realm );
        }
        else
        {
            return new BError( "Unable to create item of objtype " + hexint(itemdesc->objtype) );
        }
    }
    return new BError( "Invalid parameter type" );
}

BObjectImp* UOExecutorModule::mf_CreateItemCopyAtLocation(/* x,y,z,item,realm */)
{
    long x, y, z;
    Item* origitem;
    const String* strrealm;
    if (getParam( 0, x ) &&
        getParam( 1, y ) &&
        getParam( 2, z, ZCOORD_MIN, ZCOORD_MAX ) &&
        getItemParam( exec, 3, origitem ) &&
        getStringParam( 4, strrealm ))
    {
		if(origitem->script_isa(POLCLASS_MULTI))
			return new BError( "This function does not work with Multi objects." );

		Realm* realm = find_realm(strrealm->value());
		if(!realm)
			return new BError( "Realm not found" );

		if(!realm->valid(x,y,z)) return new BError("Invalid Coordinates for Realm");
        Item* item = origitem->clone();
        if (item != NULL)
        {
            return _complete_create_item_at_location( item, x, y, z, realm );
        }
        else
        {
            return new BError( "Unable to clone item" );
        }
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* UOExecutorModule::mf_CreateMultiAtLocation(/* x,y,z,objtype,flags,realm */)
{
    long x, y, z;
    const ItemDesc* descriptor;
    long flags = 0;
	const String* strrealm;
	Realm* realm = find_realm(string("britannia"));
    if (! (getParam( 0, x ) &&
           getParam( 1, y ) &&
           getParam( 2, z, ZCOORD_MIN, ZCOORD_MAX  ) &&
           getObjtypeParam( exec, 3, descriptor )) )
    {
        return new BError( "Invalid parameter type" );
    }
    if (exec.hasParams(5))
    {
        if (!getParam(4,flags))
            return new BError( "Invalid parameter type" );
    }
	if (exec.hasParams(6))
    {
        if (!getStringParam(5, strrealm))
            return new BError( "Invalid parameter type" );
		realm = find_realm(strrealm->value());
		if(!realm)
			return new BError( "Realm not found" );
    }
	if(!realm->valid(x,y,z)) return new BError("Invalid Coordinates for Realm");
    if (descriptor->type != ItemDesc::BOATDESC &&
        descriptor->type != ItemDesc::HOUSEDESC)
    {
        return new BError( "That objtype is not a Multi" );
    }
	if (!realm->valid(x,y,z))
    {
        return new BError( "That location is out of bounds" );
    }

    return UMulti::scripted_create( *descriptor,
                                    static_cast<unsigned short>(x),
                                    static_cast<unsigned short>(y),
                                    static_cast<signed char>(z),
                                    realm, flags );
}

/*
void replace_properties( ConfigElem& elem, ObjArray* custom )
{
    for( unsigned i = 0; i < custom->ref_arr.size(); ++i )
    {
        BObject* bo = static_cast<BObject*>( custom->ref_arr[i].get() );
        elem.clear_prop( custom->name_arr[i].c_str() );
        elem.add_prop( custom->name_arr[i].c_str(), bo->impptr()->getStringRep().c_str() );
    }
}
*/
void replace_properties( ConfigElem& elem, BStruct* custom )
{
    for( BStruct::Contents::const_iterator citr = custom->contents().begin(), end = custom->contents().end(); citr != end; ++citr )
    {
        const string& name = (*citr).first;
        const BObjectRef& ref = (*citr).second;

        elem.clear_prop( name.c_str() );
        elem.add_prop( name.c_str(), ref->impptr()->getStringRep().c_str() );
    }
}

bool FindNpcTemplate( const char *template_name,
                      ConfigFile& cfile,
                      ConfigElem& elem );
bool FindNpcTemplate( const char* template_name, ConfigElem& elem );

BObjectImp* UOExecutorModule::mf_CreateNpcFromTemplate()
{
    const String* tmplname;
    long x,y,z;
	const String* strrealm;
	Realm* realm = find_realm(string("britannia"));

    if (!(getStringParam( 0, tmplname ) &&
          getParam( 1, x  ) &&
          getParam( 2, y  ) &&
          getParam( 3, z, ZCOORD_MIN, ZCOORD_MAX  ) ))
    {
        return new BError( "Invalid parameter type" );
    }
    BObjectImp* imp = getParamImp( 4 );
    BStruct* custom_struct = NULL;
    if (imp->isa( BObjectImp::OTLong ))
    {
        custom_struct = NULL;
    }
    else if (imp->isa( BObjectImp::OTStruct ))
    {
        custom_struct = static_cast<BStruct*>(imp);
    }
    else
    {
        return new BError( string("Parameter 4 must be a Struct or Integer(0), got ") + BObjectImp::typestr( imp->type() ) );
    }
	if (exec.hasParams(6))
	{
		if(!getStringParam( 5, strrealm ))
			return new BError( "Realm not found" );
		realm = find_realm(strrealm->value());
		if(!realm) return new BError("Realm not found");
		if(!realm->valid(x,y,z)) return new BError("Invalid Coordinates for Realm");
	}
    //ConfigFile cfile;
    ConfigElem elem;
    START_PROFILECLOCK( npc_search );
    //bool found = FindNpcTemplate( tmplname->data(), cfile, elem );
    bool found = FindNpcTemplate( tmplname->data(), elem );
    STOP_PROFILECLOCK( npc_search );
    INC_PROFILEVAR( npc_searches );

    if (!found)
    {
        return new BError( "NPC template '" + tmplname->value() + "' not found" );
    }
    MOVEMODE movemode = Character::decode_movemode( elem.read_string( "MoveMode", "L" ) );

    int newz;
    UMulti* dummy_multi;
    Item* dummy_walkon;
    if (!realm->walkheight(x,y,z,&newz,&dummy_multi, &dummy_walkon, true, movemode))
    {
        return new BError( "Not a valid location for an NPC!" );
    }
    z = newz;


    NPCRef npc;
            // readProperties can throw, if stuff is missing.
    try {
        npc.set( new NPC( elem.remove_ushort( "OBJTYPE" ), elem ) );
        npc->set_dirty();
        elem.clear_prop( "Serial" );
        elem.clear_prop( "X" );
        elem.clear_prop( "Y" );
        elem.clear_prop( "Z" );

        elem.add_prop( "Serial", GetNextSerialNumber() );
        // FIXME sanity check
        elem.add_prop( "X", static_cast<unsigned short>(x) );
        elem.add_prop( "Y", static_cast<unsigned short>(y) );
        elem.add_prop( "Z", static_cast<unsigned short>(z) );
		elem.add_prop( "Realm", realm->name().c_str() );
        if (custom_struct != NULL)
            replace_properties( elem, custom_struct );
        npc->readPropertiesForNewNPC( elem );

        ////HASH
		objecthash.Insert(npc.get());
		////



        //characters.push_back( npc.get() );
        SetCharacterWorldPosition( npc.get() );

        ForEach( clients, send_char_data, static_cast<Character*>(npc.get()) );

		//dave added 2/3/3 send entered area events for npc create
		ForEachMobileInRange( static_cast<unsigned short>(x),  static_cast<unsigned short>(y), realm, 32,
			                  NpcPropagateMove, static_cast<Character*>(npc.get()) );
        if (dummy_multi)
            dummy_multi->register_object( npc.get() );
        return new ECharacterRefObjImp( npc.get() );
    }
    catch( std::exception& ex )
    {
        if (npc.get() != NULL)
            npc->destroy();
        return new BError( "Exception detected trying to create npc from template '" + tmplname->value() + "': " + ex.what() );
    }
}


BObjectImp* UOExecutorModule::mf_SubtractAmount()
{
    Item* item;
    unsigned short amount;

    if (getItemParam( exec, 0, item ) &&
        getParam( 1, amount, 1, MAX_STACK_ITEMS ))
    {
        if (item->inuse() && !is_reserved_to_me(item))
        {
            return new BError( "That item is being used." );
        }
        subtract_amount_from_item( item, amount );
        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* UOExecutorModule::mf_AddAmount()
{
    Item* item;
    unsigned short amount;

    if (getItemParam( exec, 0, item ) &&
        getParam( 1, amount, 1, MAX_STACK_ITEMS ))
    {
        if (item->inuse() && !is_reserved_to_me(item))
        {
            return new BError( "That item is being used." );
        }
        if (~tile_flags( item->graphic ) & FLAG::STACKABLE)
        {
            return new BError( "That item type is not stackable." );
        }
        if (!item->can_add_to_self(amount))
        {
            return new BError( "Can't add that much to that stack" );
        }

        unsigned short newamount = item->getamount();
        newamount += amount;
        item->setamount( newamount );
        update_item_to_inrange( item );

		//DAVE added this 12/05: if in a Character's pack, update weight.
		UpdateCharacterWeight(item);

        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

/*
BObjectImp* UOExecutorModule::mf_GetSkill()
{
    Character* chr;
    USKILLID skillid;
    if (getCharacterParam( 0, chr ) &&
        getSkillIdParam( exec, 1, skillid ))
    {
        return new BLong( chr->get_skill( skillid ) );
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}
*/
/*
BObjectImp* UOExecutorModule::mf_GetRawSkill()
{
    Character* chr;
    USKILLID skillid;
    if (getCharacterParam( 0, chr ) &&
        getSkillIdParam( exec, 1, skillid ))
    {
        return new BLong( chr->get_raw_skill( skillid ) );
    }
    else
    {
        return new BError( "Invalid Parameter" );
    }
}
*/
/*
BObjectImp* UOExecutorModule::mf_SetRawSkill()
{
    Character* chr;
    USKILLID skillid;
    long rawskill;
    if (getCharacterParam( 0, chr ) &&
        getSkillIdParam( exec, 1, skillid ) &&
        getParam( 2, rawskill ))
    {
        if (rawskill >= 0)
        {
            chr->set_raw_skill( skillid, rawskill );
            return new BLong( 1 );
        }
        else
        {
            return new BError( "Raw Skill Values must be positive" );
        }
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}
*/

/*
BObjectImp* UOExecutorModule::mf_AwardRawPoints()
{
    Character* chr;
    USKILLID skillid;
    long points;

    if (getCharacterParam( 0, chr ) &&
        getSkillIdParam( exec, 1, skillid ) &&
        getParam( 2, points ))
    {
        if (points < 0 || points > USHRT_MAX)
            return new BError( "Points to award is out of range" );

        chr->award_raw_skillpoints( skillid, points );
        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}
*/

BObjectImp* UOExecutorModule::mf_PerformAction()
{
    Character* chr;
    unsigned short actionval;
    if (getCharacterParam( exec, 0, chr ) &&
        getParam( 1, actionval ))
    {
        UACTION action = static_cast<UACTION>(actionval);
        send_action_to_inrange( chr, action );
        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_PlaySoundEffect()
{
    UObject* chr;
    long effect;
    if (getUObjectParam( exec, 0, chr ) &&
        getParam( 1, effect ))
    {
        play_sound_effect( chr, static_cast<u16>(effect) );
        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_PlaySoundEffectPrivate()
{
    UObject* center;
    long effect;
    Character* forchr;
    if (getUObjectParam( exec, 0, center ) &&
        getParam( 1, effect ) &&
        getCharacterParam( exec, 2, forchr ) )
    {
        play_sound_effect_private( center, static_cast<u16>(effect), forchr );
        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

void menu_selection_made( Client* client, MenuItem* mi, MSG7D_MENU_SELECTION* msg )
{
    if (client != NULL)
    {
        Character* chr = client->chr;
        if (chr != NULL &&
            chr->client->gd->menu_selection_uoemod != NULL)
        {
            if (mi != NULL &&
                msg != NULL)
            {
                BStruct* selection = new BStruct;
                        // FIXME should make sure objtype and choice are within valid range.
                selection->addMember( "objtype", new BLong( mi->objtype_ ) );
                selection->addMember( "graphic", new BLong( mi->graphic_ ) );
                selection->addMember( "index", new BLong( cfBEu16(msg->choice) ) ); // this has been validated
                selection->addMember( "color", new BLong( mi->color_ ) );
                chr->client->gd->menu_selection_uoemod->uoexec.ValueStack.top().set( new BObject( selection ) );
            }
            // 0 is already on the value stack, for the case of cancellation.
            chr->client->gd->menu_selection_uoemod->uoexec.os_module->revive();
            chr->client->gd->menu_selection_uoemod->menu_selection_chr = NULL;
            chr->client->gd->menu_selection_uoemod = NULL;
        }
    }
}

bool UOExecutorModule::getDynamicMenuParam( unsigned param, Menu*& menu )
{
/*
    const String* pmenuname;
    if (getStringParam( param, pmenuname ))
    {
        menu = find_menu( pmenuname->data() );
        return (menu != NULL);
    }
    return false;
*/
	BApplicObjBase* aob = getApplicObjParam( param, &menu_type );
    if (aob != NULL)
    {
        EMenuObjImp* menu_imp = static_cast<EMenuObjImp*>(aob);
        menu = &menu_imp->value();
        return true;
    }
    else
    {
        return false;
    }
}

bool UOExecutorModule::getStaticOrDynamicMenuParam( unsigned param, Menu*& menu )
{
    BObjectImp* imp = getParamImp( param );
    if (imp->isa( BObjectImp::OTString ))
    {
        String* pmenuname = static_cast<String*>(imp);
        menu = find_menu( pmenuname->data() );
        return (menu != NULL);
    }
    else if (imp->isa( BObjectImp::OTApplicObj ))
    {
        BApplicObjBase* aob = static_cast<BApplicObjBase*>(imp);
        if (aob->object_type() == &menu_type)
        {
            EMenuObjImp* menu_imp = static_cast<EMenuObjImp*>(aob);
            menu = &menu_imp->value();
            return true;
        }
    }
    if (mlog.is_open())
        mlog << "SelectMenuItem: expected a menu name (static menu) or a CreateMenu() dynamic menu" << endl;
    return false;
}

BObjectImp* UOExecutorModule::mf_SelectMenuItem()
{
    Character* chr;
    Menu* menu;

    if (getCharacterParam( exec, 0, chr ) &&
        getStaticOrDynamicMenuParam( 1, menu ) &&
        (chr->client->gd->menu_selection_uoemod == NULL))
    {
        if (menu != NULL &&
            chr->has_active_client() &&
            !menu->menuitems_.empty())
        {
            if (send_menu( chr->client, menu ))
            {
                chr->menu = menu;
                chr->on_menu_selection = menu_selection_made;
                chr->client->gd->menu_selection_uoemod = this;
                menu_selection_chr = chr;
                uoexec.os_module->suspend();
                return new BLong(0);
            }
            else
            {
                return new BError( "Menu too large" );
            }
        }
        else
        {
            return new BError( "Client is busy, or menu is empty" );
        }
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

void append_objtypes( ObjArray* objarr, Menu* menu )
{
    for( unsigned i = 0; i < menu->menuitems_.size(); ++i )
    {
        MenuItem* mi = &menu->menuitems_[i];

        if (mi->submenu_id)
        {
            Menu* menu = find_menu( mi->submenu_id );
            if (menu != NULL)
                append_objtypes( objarr, menu );
        }
        else
        {
            objarr->addElement( new BLong( mi->objtype_ ) );
        }
    }
}

BObjectImp* UOExecutorModule::mf_GetMenuObjTypes()
{
    Menu* menu;
    if (getStaticOrDynamicMenuParam( 0, menu ))
    {
        auto_ptr<ObjArray> objarr( new ObjArray );

        append_objtypes( objarr.get(), menu );

        return objarr.release();
    }
    return new BLong(0);
}

BObjectImp* UOExecutorModule::mf_ApplyConstraint()
{
    ObjArray* arr;
    StoredConfigFile* cfile;
    const String* propname_str;
    long amthave;

    if (getObjArrayParam( 0, arr ) &&
        getStoredConfigFileParam( *this, 1, cfile ) &&
        getStringParam( 2, propname_str ) &&
        getParam( 3, amthave ))
    {
        auto_ptr<ObjArray> newarr( new ObjArray );

        for( unsigned i = 0; i < arr->ref_arr.size(); i++ )
        {
            BObjectRef& ref = arr->ref_arr[ i ];

            BObject *bo = ref.get();
            if (bo == NULL)
                continue;
            if (!bo->isa( BObjectImp::OTLong ))
                continue;
            BLong* blong = static_cast<BLong*>(bo->impptr());
            unsigned long objtype = blong->value();

            ref_ptr<StoredConfigElem> celem = cfile->findelem( objtype );
            if (celem.get() == NULL)
                continue;

            BObjectImp* propval = celem->getimp( propname_str->value() );
            if (propval == NULL)
                continue;
            if (!propval->isa( BObjectImp::OTLong))
                continue;
            BLong* amtreq = static_cast<BLong*>(propval);
            if (amtreq->value() > amthave)
                continue;
            newarr->addElement( new BLong( objtype ) );
        }

        return newarr.release();
    }

    return new UninitObject;
}

BObjectImp* UOExecutorModule::mf_CreateMenu()
{
    const String* title;
    if (getStringParam( 0, title ))
    {
        Menu temp;
        temp.menu_id = 0;
        strzcpy( temp.title, title->data(), sizeof temp.title );
        return new EMenuObjImp( temp );
    }
    return new BLong(0);
}

BObjectImp* UOExecutorModule::mf_AddMenuItem()
{
    Menu* menu;
    unsigned short objtype;
    const String* text;
    unsigned short color;

    if (getDynamicMenuParam( 0, menu ) &&
        getObjtypeParam( exec, 1, objtype ) &&
        getStringParam( 2, text ) &&
		getParam( 3, color ) )
    {
        menu->menuitems_.push_back( MenuItem() );
        MenuItem* mi = &menu->menuitems_.back();
        mi->objtype_ = objtype;
        mi->graphic_ = getgraphic( objtype );
        strzcpy( mi->title, text->data(), sizeof mi->title );
		mi->color_ = color & ssopt.item_color_mask;
        return new BLong(1);
    }
    else
    {
        return new BLong(0);
    }
}

BObjectImp* UOExecutorModule::mf_GetObjProperty()
{
    UObject* uobj;
    const String* propname_str;
    if (getUObjectParam( exec, 0, uobj ) &&
        getStringParam( 1, propname_str ))
    {
        std::string val;
        if (uobj->getprop( propname_str->value(), val ))
        {
            return BObjectImp::unpack( val.c_str() );
        }
        else
        {
            return new BError( "Property not found" );
        }
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* UOExecutorModule::mf_SetObjProperty()
{
    UObject* uobj;
    const String* propname_str;
    if (getUObjectParam( exec, 0, uobj ) &&
        getStringParam( 1, propname_str ))
    {
        BObjectImp* propval = getParamImp( 2 );
        uobj->setprop( propname_str->value(), propval->pack() );
        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* UOExecutorModule::mf_EraseObjProperty()
{
    UObject* uobj;
    const String* propname_str;
    if (getUObjectParam( exec, 0, uobj ) &&
        getStringParam( 1, propname_str ))
    {
        uobj->eraseprop( propname_str->value() );
        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}
BObjectImp* UOExecutorModule::mf_GetObjPropertyNames()
{
    UObject* uobj;
    if (getUObjectParam( exec, 0, uobj ))
    {
        vector<string> propnames;
        uobj->getpropnames( propnames );
        ObjArray* arr = new ObjArray;
        for( unsigned i = 0; i < propnames.size(); ++i )
        {
            arr->addElement( new String(propnames[i]) );
        }
        return arr;
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

#include "gprops.h"
BObjectImp* UOExecutorModule::mf_GetGlobalProperty()
{
    const String* propname_str;
    if (getStringParam( 0, propname_str ))
    {
        std::string val;
        if (global_properties.getprop( propname_str->value(), val ))
        {
            return BObjectImp::unpack( val.c_str() );
        }
        else
        {
            return new BError( "Property not found" );
        }
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* UOExecutorModule::mf_SetGlobalProperty()
{
    const String* propname_str;
    if (exec.getStringParam( 0, propname_str ))
    {
        BObjectImp* propval = exec.getParamImp( 1 );
        global_properties.setprop( propname_str->value(), propval->pack() );
        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* UOExecutorModule::mf_EraseGlobalProperty()
{
    const String* propname_str;
    if (getStringParam( 0, propname_str ))
    {
        global_properties.eraseprop( propname_str->value() );
        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* UOExecutorModule::mf_PlayMovingEffect()
{
    UObject* src;
    UObject* dst;
    unsigned short effect;
    long speed;
    long loop;
    long explode;
    if (getUObjectParam( exec, 0, src ) &&
        getUObjectParam( exec, 1, dst ) &&
        getParam( 2, effect ) &&
        getParam( 3, speed, UCHAR_MAX ) &&
        getParam( 4, loop, UCHAR_MAX ) &&
        getParam( 5, explode, UCHAR_MAX ))
    {
		if(src->realm != dst->realm)
			return new BError("Realms must match");
        play_moving_effect( src, dst, effect,
                            static_cast<unsigned char>(speed),
                            static_cast<unsigned char>(loop),
                            static_cast<unsigned char>(explode),
							src->realm );
        return new BLong(1);
    }
    else
    {
        return new BLong(0);
    }
}

BObjectImp* UOExecutorModule::mf_PlayMovingEffectXyz()
{
    long sx, sy, sz;
    long dx, dy, dz;

    unsigned short effect;
    long speed;
    long loop;
    long explode;
	const String* strrealm;
	Realm* realm = find_realm(string("britannia"));

    if (getParam( 0, sx ) &&
        getParam( 1, sy ) &&
        getParam( 2, sz ) &&
        getParam( 3, dx ) &&
        getParam( 4, dy ) &&
        getParam( 5, dz ) &&
        getParam( 6, effect ) &&
        getParam( 7, speed, UCHAR_MAX ) &&
        getParam( 8, loop, UCHAR_MAX ) &&
        getParam( 9, explode, UCHAR_MAX ) &&
		getStringParam( 10, strrealm) )
    {
		realm = find_realm(strrealm->value());
		if(!realm) return new BError("Realm not found");
		if(!realm->valid(sx,sy,sz) || !realm->valid(dx,dy,dz))
			return new BError("Invalid Coordinates for realm");
        play_moving_effect2( static_cast<unsigned short>(sx),
                             static_cast<unsigned short>(sy),
                             static_cast<signed char>(sz),
                             static_cast<unsigned short>(dx),
                             static_cast<unsigned short>(dy),
                             static_cast<signed char>(dz),
                             effect,
                             static_cast<signed char>(speed),
                             static_cast<signed char>(loop),
                             static_cast<signed char>(explode),
							 realm);
        return new BLong(1);
    }
    else
    {
        return NULL;
    }
}

// FIXME add/test:stationary

BObjectImp* UOExecutorModule::mf_PlayObjectCenteredEffect()
{
    UObject* src;
    unsigned short effect;
    long speed;
    long loop;
    if (getUObjectParam( exec, 0, src ) &&
        getParam( 1, effect ) &&
        getParam( 2, speed, UCHAR_MAX ) &&
        getParam( 3, loop, UCHAR_MAX ))
    {
        play_object_centered_effect( src,
                                     effect,
                                     static_cast<unsigned char>(speed),
                                     static_cast<unsigned char>(loop) );
        return new BLong(1);
    }
    else
    {
        return NULL;
    }
}

BObjectImp* UOExecutorModule::mf_PlayStationaryEffect()
{
    long x, y, z;
    unsigned short effect;
    long speed, loop, explode;
	const String* strrealm;
	Realm* realm = find_realm(string("britannia"));
    if (getParam( 0, x ) &&
        getParam( 1, y ) &&
        getParam( 2, z ) &&
        getParam( 3, effect ) &&
        getParam( 4, speed, UCHAR_MAX ) &&
        getParam( 5, loop, UCHAR_MAX ) &&
        getParam( 6, explode, UCHAR_MAX ) &&
		getStringParam( 7, strrealm))
    {
		realm = find_realm(strrealm->value());
		if(!realm) return new BError("Realm not found");
		if(!realm->valid(x,y,z)) return new BError("Invalid Coordinates for realm");

        play_stationary_effect( static_cast<unsigned short>(x),
                                static_cast<unsigned short>(y),
                                static_cast<signed char>(z),
                                effect,
                                static_cast<unsigned char>(speed),
                                static_cast<unsigned char>(loop),
                                static_cast<unsigned char>(explode),
								realm );
        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_PlayLightningBoltEffect()
{
    UObject* center;
    if (getUObjectParam( exec, 0, center ))
    {
        play_lightning_bolt_effect( center );
        return new BLong(1);
    }
    else
    {
        return new BLong(0);
    }
}

BObjectImp* UOExecutorModule::mf_ListItemsNearLocation(/* x, y, z, range, realm */)
{
    long x, y, z, range;
	const String* strrealm;
	Realm* realm;

    if (getParam( 0, x ) &&
        getParam( 1, y ) &&
        getParam( 2, z ) &&
        getParam( 3, range ) &&
		getStringParam( 4, strrealm) )
    {
		realm = find_realm(strrealm->value());
		if (!realm)
			return new BError("Realm not found");

        if (z == LIST_IGNORE_Z)
        {
            if (!realm->valid(x, y, 0))
                return new BError("Invalid Coordinates for realm");
        }
        else
        {
            if (!realm->valid(x, y, z))
                return new BError("Invalid Coordinates for realm");
        }

        auto_ptr<ObjArray> newarr( new ObjArray );

        unsigned short wxL, wyL, wxH, wyH;
        zone_convert_clip( x - range, y - range, realm, wxL, wyL );
        zone_convert_clip( x + range, y + range, realm, wxH, wyH );
        
        for( unsigned short wx = wxL; wx <= wxH; ++wx )
        {
            for( unsigned short wy = wyL; wy <= wyH; ++wy )
            {
                ZoneItems& witem = realm->zone[wx][wy].items;
                for( ZoneItems::iterator itr = witem.begin(), end = witem.end(); itr != end; ++itr )
                {
		            Item* item = *itr;

                    if ((abs(item->x - x) <= range) && (abs(item->y - y) <= range))
                        if ((z == LIST_IGNORE_Z) || (abs(item->z - z) < CONST_DEFAULT_ZRANGE))
                            newarr->addElement( item->make_ref() );
                }
            }
        }

        return newarr.release();
    }

    return NULL;
}

void UOExecutorModule::internal_InBoxAreaChecks(long &x1, long &y1, long &z1, long &x2, long &y2, long &z2, Realm* realm)
{
	if( x1 < 0 )
		x1 = 0;
	if ( y1 < 0 )
		y1 = 0;
	if ( z1 < WORLD_MIN_Z )
		z1 = WORLD_MIN_Z;

	if( unsigned(x2) >= realm->width() )
		x2 = (realm->width()-1);
	if ( unsigned(y2) >= realm->height() )
		y2 = (realm->height()-1);
	if ( z2 > WORLD_MAX_Z )
		z2 = WORLD_MAX_Z;
}

BObjectImp* UOExecutorModule::mf_ListObjectsInBox(/* x1, y1, z1, x2, y2, z2, realm */)
{
    long x1, y1, z1;
    long x2, y2, z2;
	const String* strrealm;
	Realm* realm;

    if (!(getParam( 0, x1 ) &&
          getParam( 1, y1 ) &&
          getParam( 2, z1 ) &&
          getParam( 3, x2 ) &&
          getParam( 4, y2 ) &&
          getParam( 5, z2 ) &&
		  getStringParam( 6, strrealm) ))
    {
        return new BError( "Invalid parameter" );
    }

	realm = find_realm(strrealm->value());
	if (!realm)
        return new BError("Realm not found");

	// Disabled again: ShardAdmins "loves" this "bug" :o/
	// if ((!realm->valid(x1, y1, z1)) || (!realm->valid(x2, y2, z2)))
	//     return new BError("Invalid Coordinates for realm");
	internal_InBoxAreaChecks(x1, y1, z1, x2, y2, z2, realm);

    auto_ptr<ObjArray> newarr( new ObjArray );

    unsigned short wxL, wyL, wxH, wyH;
    zone_convert_clip( x1, y1, realm, wxL, wyL );
    zone_convert_clip( x2, y2, realm, wxH, wyH );

    for( unsigned short wx = wxL; wx <= wxH; ++wx )
    {
        for( unsigned short wy = wyL; wy <= wyH; ++wy )
        {
            ZoneCharacters& wchr = realm->zone[wx][wy].characters;
            for( ZoneCharacters::iterator itr = wchr.begin(), end = wchr.end(); itr != end; ++itr )
            {
                Character* chr = *itr;
                if (chr->x >= x1 && chr->x <= x2 &&
                    chr->y >= y1 && chr->y <= y2 &&
                    chr->z >= z1 && chr->z <= z2)
                {
                    newarr->addElement( chr->make_ref() );
                }
            }

            ZoneItems& witem = realm->zone[wx][wy].items;
            for( ZoneItems::iterator itr = witem.begin(), end = witem.end(); itr != end; ++itr )
            {
                Item* item = *itr;
                if (item->x >= x1 && item->x <= x2 &&
                    item->y >= y1 && item->y <= y2 &&
                    item->z >= z1 && item->z <= z2)
                {
                    newarr->addElement( item->make_ref() );
                }
            }
        }
    }

    return newarr.release();
}

BObjectImp* UOExecutorModule::mf_ListMultisInBox(/* x1, y1, z1, x2, y2, z2, realm */)
{
    long x1, y1, z1;
    long x2, y2, z2;
	const String* strrealm;
	Realm* realm;

    if (!(getParam( 0, x1 ) &&
          getParam( 1, y1 ) &&
          getParam( 2, z1 ) &&
          getParam( 3, x2 ) &&
          getParam( 4, y2 ) &&
          getParam( 5, z2 ) &&
		  getStringParam( 6, strrealm) ))
    {
        return new BError( "Invalid parameter" );
    }
	
    realm = find_realm(strrealm->value());
	if (!realm)
        return new BError("Realm not found");

	// Disabled again: ShardAdmins "loves" this "bug" :o/
	// if ((!realm->valid(x1, y1, z1)) || (!realm->valid(x2, y2, z2)))
	//     return new BError("Invalid Coordinates for realm");
	internal_InBoxAreaChecks(x1, y1, z1, x2, y2, z2, realm);

    auto_ptr<ObjArray> newarr( new ObjArray );

    unsigned short wxL, wyL, wxH, wyH;
    zone_convert_clip( x1+MultiDef::global_minrx, y1+MultiDef::global_minry, realm, wxL, wyL );
    zone_convert_clip( x2+MultiDef::global_maxrx, y2+MultiDef::global_maxry, realm, wxH, wyH );

    // search for multis.  this is tricky, since the center might lie outside the box
    for( unsigned short wx = wxL; wx <= wxH; ++wx )
    {
        for( unsigned short wy = wyL; wy <= wyH; ++wy )
        {
            ZoneMultis& multis = realm->zone[wx][wy].multis;
            for( ZoneMultis::const_iterator citr = multis.begin(), end = multis.end(); citr != end; ++citr )
            {
                UMulti* multi = *citr;
                const MultiDef& md = multi->multidef();
                if (multi->x + md.minrx > x2 || // east of the box
                    multi->x + md.maxrx < x1 || // west of the box
                    multi->y + md.minry > y2 || // south of the box
                    multi->y + md.maxry < y1 || // north of the box
                    multi->z + md.minrz > z2 || // above the box
                    multi->z + md.maxrz < z1) // below the box
                {
                    continue;
                }

                // some part of it is contained in the box.  Look at the individual statics, to see
                // if any of them lie within.
                for( MultiDef::Components::const_iterator citr = md.components.begin(), end = md.components.end();
					 citr != end;
					 ++citr )
                {
                    const MULTI_ELEM* elem = (*citr).second;
                    int absx = multi->x + elem->x;
                    int absy = multi->y + elem->y;
                    int absz = multi->z + elem->z;
                    if (x1 <= absx && absx <= x2 &&
                        y1 <= absy && absy <= y2)
                    {
                        // do Z checking
                        int height = tileheight( getgraphic( elem->objtype) );
                        int top = absz + height;

                        if (z1 <= absz && absz <= z2  ||    // bottom point lies between
                            z1 <= top && top <= z2 ||       // top lies between
                            top >= z2 && absz <= z1)        // spans
                        {
                            newarr->addElement( multi->make_ref() );
                            break; // out of for
                        }
                    }
                }
            }
        }
    }

    return newarr.release();
}

BObjectImp* UOExecutorModule::mf_ListStaticsInBox(/* x1, y1, z1, x2, y2, z2, flags, realm */)
{
    long x1, y1, z1;
    long x2, y2, z2, flags;
	const String* strrealm;
	Realm* realm;

    if (getParam( 0, x1 ) &&
        getParam( 1, y1 ) &&
        getParam( 2, z1 ) &&
        getParam( 3, x2 ) &&
        getParam( 4, y2 ) &&
        getParam( 5, z2 ) &&
        getParam( 6, flags ) &&
        getStringParam( 7, strrealm) )
    {
        realm = find_realm(strrealm->value());
        if (!realm)
            return new BError("Realm not found");

		// Disabled again: ShardAdmins "loves" this "bug" :o/
		// if ((!realm->valid(x1, y1, z1)) || (!realm->valid(x2, y2, z2)))
		//     return new BError("Invalid Coordinates for realm");
		internal_InBoxAreaChecks(x1, y1, z1, x2, y2, z2, realm);

		auto_ptr<ObjArray> newarr( new ObjArray );

        for( long wx = x1; wx <= x2; ++wx )
        {
            for( long wy = y1; wy <= y2; ++wy )
            {
                if (!( flags & ITEMS_IGNORE_STATICS ))
                {
                    StaticEntryList slist;
                    realm->getstatics( slist, wx, wy );

                    for( unsigned i = 0; i < slist.size(); ++i )
                    {
                        if ((z1 <= slist[i].z) && (slist[i].z <= z2))
                        {
                            BStruct* arr = new BStruct;
                            arr->addMember( "x", new BLong( wx ) );
                            arr->addMember( "y", new BLong( wy ) );
                            arr->addMember( "z", new BLong( slist[i].z ) );
                            arr->addMember( "objtype", new BLong( slist[i].objtype ) );
                            newarr->addElement( arr );
                        }
                    }
                }

                if (!( flags & ITEMS_IGNORE_MULTIS ))
                {
                    StaticList mlist;
                    realm->readmultis( mlist, wx, wy );

                    for( unsigned i = 0; i < mlist.size(); ++i )
                    {
                        if ((z1 <= mlist[i].z) && (mlist[i].z <= z2))
                        {
                            BStruct* arr = new BStruct;
                            arr->addMember( "x", new BLong( wx ) );
                            arr->addMember( "y", new BLong( wy ) );
                            arr->addMember( "z", new BLong( mlist[i].z ) );
                            arr->addMember( "objtype", new BLong( mlist[i].graphic ) );
                            newarr->addElement( arr );
                        }
                    }
                }
            }
        }
        
        return newarr.release();
    }
    else
        return new BError( "Invalid parameter" );
}

BObjectImp* UOExecutorModule::mf_ListItemsNearLocationOfType(/* x, y, z, range, objtype, realm */)
{
    long x, y, z, range;
    unsigned short objtype;
	const String* strrealm;
	Realm* realm;

    if (getParam( 0, x ) &&
        getParam( 1, y ) &&
        getParam( 2, z ) &&
        getParam( 3, range ) &&
        getObjtypeParam( exec, 4, objtype ) &&
		getStringParam(5, strrealm) )
    {
		realm = find_realm(strrealm->value());
		if (!realm)
            return new BError("Realm not found");

        auto_ptr<ObjArray> newarr( new ObjArray );

        if (z == LIST_IGNORE_Z)
        {
            if (!realm->valid(x, y, 0))
                return new BError("Invalid Coordinates for realm");
        }
        else
        {
            if (!realm->valid(x, y, z))
                return new BError("Invalid Coordinates for realm");
        }

        unsigned short wxL, wyL, wxH, wyH;
        zone_convert_clip( x - range, y - range, realm, wxL, wyL );
        zone_convert_clip( x + range, y + range, realm, wxH, wyH );

        for( unsigned short wx = wxL; wx <= wxH; ++wx )
        {
            for( unsigned short wy = wyL; wy <= wyH; ++wy )
            {
                ZoneItems& witem = realm->zone[wx][wy].items;
                for( ZoneItems::iterator itr = witem.begin(), end = witem.end(); itr != end; ++itr )
                {
                    Item* item = *itr;

                    if ((item->objtype_ == objtype) && (abs(item->x - x) <= range) && (abs(item->y - y) <= range))
                        if ((z == LIST_IGNORE_Z) || (abs(item->z - z) < CONST_DEFAULT_ZRANGE))
                            newarr->addElement( item->make_ref() );
                }
            }
        }

        return newarr.release();
    }

    return NULL;
}


BObjectImp* UOExecutorModule::mf_ListItemsAtLocation(/* x, y, z, realm */)
{
    long x, y, z;
	const String* strrealm;
	Realm* realm;

    if (getParam( 0, x ) &&
        getParam( 1, y ) &&
        getParam( 2, z ) &&
		getStringParam( 3, strrealm) )
    {
		realm = find_realm(strrealm->value());
		if (!realm)
            return new BError("Realm not found");

        if (z == LIST_IGNORE_Z)
        {
            if (!realm->valid(x, y, 0))
                return new BError("Invalid Coordinates for realm");
        }
        else
        {
            if (!realm->valid(x, y, z))
                return new BError("Invalid Coordinates for realm");
        }

        auto_ptr<ObjArray> newarr( new ObjArray );

        unsigned short wx, wy;
        zone_convert_clip( x, y, realm, wx, wy );
        ZoneItems& witem = realm->zone[wx][wy].items;
        
        for( ZoneItems::iterator itr = witem.begin(), end = witem.end(); itr != end; ++itr )
        {
            Item* item = *itr;

            if ((item->x == x) && (item->y == y))
                if ((z == LIST_IGNORE_Z) || (item->z == z))
                    newarr->addElement( item->make_ref() );
        }

        return newarr.release();
    }

    return NULL;
}

BObjectImp* UOExecutorModule::mf_ListGhostsNearLocation()
{
    long x;
    long y;
    long z;
    long range;
	const String* strrealm;
	Realm* realm = find_realm(string("britannia"));

    if (getParam( 0, x ) &&
        getParam( 1, y ) &&
        getParam( 2, z ) &&
        getParam( 3, range ) &&
		getStringParam( 4, strrealm) )
    {
		realm = find_realm(strrealm->value());
		if(!realm) return new BError("Realm not found");

        auto_ptr<ObjArray> newarr( new ObjArray );

        unsigned short wxL, wyL, wxH, wyH;
        zone_convert_clip( x - range, y - range, realm, wxL, wyL );
        zone_convert_clip( x + range, y + range, realm, wxH, wyH );
        for( unsigned short wx = wxL; wx <= wxH; ++wx )
        {
            for( unsigned short wy = wyL; wy <= wyH; ++wy )
            {
                ZoneCharacters& wchr = realm->zone[wx][wy].characters;

                for( ZoneCharacters::iterator itr = wchr.begin(), end = wchr.end(); itr != end; ++itr )
                {
                    Character* chr = *itr;

                    if (chr->dead() &&
                        (abs(chr->z - z) < CONST_DEFAULT_ZRANGE) &&
                        (abs(chr->x - x) <= range) &&
                        (abs(chr->y - y) <= range))
                    {
                        newarr->addElement( chr->make_ref() );
                    }
                }
            }
        }

        return newarr.release();
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

const unsigned LMBLEX_FLAG_NORMAL = 0x01;
const unsigned LMBLEX_FLAG_HIDDEN = 0x02;
const unsigned LMBLEX_FLAG_DEAD   = 0x04;
const unsigned LMBLEX_FLAG_CONCEALED = 0x8;

BObjectImp* UOExecutorModule::mf_ListMobilesNearLocationEx(/* x, y, z, range, flags, realm */)
{
    long x, y, z, range, flags;
	const String* strrealm;
	Realm* realm;

    if (getParam( 0, x ) &&
        getParam( 1, y ) &&
        getParam( 2, z ) &&
        getParam( 3, range ) &&
        getParam( 4, flags ) &&
		getStringParam(5, strrealm) )
    {
		realm = find_realm(strrealm->value());
		if (!realm)
            return new BError("Realm not found");

        if (z == LIST_IGNORE_Z)
        {
            if (!realm->valid(x, y, 0))
                return new BError("Invalid Coordinates for realm");
        }
        else
        {
            if (!realm->valid(x, y, z))
                return new BError("Invalid Coordinates for realm");
        }

        bool inc_normal = (flags & LMBLEX_FLAG_NORMAL)? true:false;
        bool inc_hidden = (flags & LMBLEX_FLAG_HIDDEN) ? true:false;
        bool inc_dead = (flags & LMBLEX_FLAG_DEAD)? true:false;
        bool inc_concealed = (flags & LMBLEX_FLAG_CONCEALED)? true:false;
        
        auto_ptr<ObjArray> newarr(new ObjArray);

        unsigned short wxL, wyL, wxH, wyH;
        zone_convert_clip( x - range, y - range, realm, wxL, wyL );
        zone_convert_clip( x + range, y + range, realm, wxH, wyH );

        for( unsigned short wx = wxL; wx <= wxH; ++wx )
        {
            for( unsigned short wy = wyL; wy <= wyH; ++wy )
            {
                ZoneCharacters& wchr = realm->zone[wx][wy].characters;

                for( ZoneCharacters::iterator itr = wchr.begin(), end = wchr.end(); itr != end; ++itr )
                {
                    Character* chr = *itr;

                    if ( (inc_hidden && chr->hidden()) ||
                        (inc_dead   && chr->dead()) ||
                        (inc_concealed && chr->concealed()) ||
                        (inc_normal && !(chr->hidden()||chr->dead()||chr->concealed())) )
                        if ((abs(chr->x - x) <= range) && (abs(chr->y - y) <= range))
                            if ((z == LIST_IGNORE_Z) || (abs(chr->z - z) < CONST_DEFAULT_ZRANGE))
                                newarr->addElement( chr->make_ref() );
                }
            }
        }

        return newarr.release();
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_ListMobilesNearLocation(/* x, y, z, range, realm */)
{
    long x, y, z, range;
	const String* strrealm;
	Realm* realm;

    if (getParam( 0, x ) &&
        getParam( 1, y ) &&
        getParam( 2, z ) &&
        getParam( 3, range ) &&
		getStringParam( 4, strrealm) )
    {
		realm = find_realm(strrealm->value());
		if (!realm)
            return new BError("Realm not found");

        if (z == LIST_IGNORE_Z)
        {
            if (!realm->valid(x, y, 0))
                return new BError("Invalid Coordinates for realm");
        }
        else
        {
            if (!realm->valid(x, y, z))
                return new BError("Invalid Coordinates for realm");
        }

        auto_ptr<ObjArray> newarr( new ObjArray );

        unsigned short wxL, wyL, wxH, wyH;
        zone_convert_clip( x - range, y - range, realm, wxL, wyL );
        zone_convert_clip( x + range, y + range, realm, wxH, wyH );

        for( unsigned short wx = wxL; wx <= wxH; ++wx )
        {
            for( unsigned short wy = wyL; wy <= wyH; ++wy )
            {
                ZoneCharacters& wchr = realm->zone[wx][wy].characters;

                for( ZoneCharacters::iterator itr = wchr.begin(), end = wchr.end(); itr != end; ++itr )
                {
                    Character* chr = *itr;

                    if ((!chr->concealed()) && (!chr->hidden()) && (!chr->dead()) &&
                        (abs(chr->x - x) <= range) && (abs(chr->y - y) <= range))
                        if ((z == LIST_IGNORE_Z) || (abs(chr->z - z) < CONST_DEFAULT_ZRANGE))
                            newarr->addElement( chr->make_ref() );
                }
            }
        }

        return newarr.release();
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_ListMobilesInLineOfSight()
{
    UObject* obj;
    long range;
    if (getUObjectParam( exec, 0, obj ) &&
        getParam( 1, range ))
    {
        obj = obj->toplevel_owner();
        auto_ptr<ObjArray> newarr( new ObjArray );

        unsigned short wxL, wyL, wxH, wyH;
        zone_convert_clip( obj->x - range, obj->y - range, obj->realm, wxL, wyL );
        zone_convert_clip( obj->x + range, obj->y + range, obj->realm, wxH, wyH );
        for( unsigned short wx = wxL; wx <= wxH; ++wx )
        {
            for( unsigned short wy = wyL; wy <= wyH; ++wy )
            {
                ZoneCharacters& wchr = obj->realm->zone[wx][wy].characters;

                for( ZoneCharacters::iterator itr = wchr.begin(), end = wchr.end(); itr != end; ++itr )
                {
                    Character* chr = *itr;
                    if (chr->dead() || chr->hidden() || chr->concealed())
                        continue;
                    if (chr == obj)
                        continue;
                    if ((abs(chr->x - obj->x) <= range) &&
                        (abs(chr->y - obj->y) <= range) &&
                        (abs(chr->z - obj->z) < CONST_DEFAULT_ZRANGE))
                    {
                        if (obj->realm->has_los( *obj, *chr ))
                        {
                            newarr->addElement( chr->make_ref() );
                        }
                    }
                }
            }
        }
        return newarr.release();
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

// keep this in sync with UO.EM
const long LH_FLAG_LOS = 1;             // only include those in LOS
const long LH_FLAG_INCLUDE_HIDDEN = 2;  // include hidden characters
BObjectImp* UOExecutorModule::mf_ListHostiles()
{
    Character* chr;
    long range;
    long flags;
    if (getCharacterParam( exec, 0, chr ) &&
        getParam( 1, range ) &&
        getParam( 2, flags ))
    {
        auto_ptr<ObjArray> arr(new ObjArray);

        const Character::CharacterSet& cont = chr->hostiles();
        Character::CharacterSet::const_iterator itr = cont.begin(), end = cont.end();
        for( ; itr != end; ++itr )
        {
            Character* hostile = *itr;
            if (hostile->concealed())
                continue;
            if ((flags & LH_FLAG_LOS) && !chr->realm->has_los( *chr, *hostile ))
                continue;
            if ((~flags & LH_FLAG_INCLUDE_HIDDEN) && hostile->hidden())
                continue;
            if (!inrangex(chr,hostile,range))
                continue;
            arr->addElement( hostile->make_ref() );
        }

        return arr.release();
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_CheckLineOfSight()
{
    UObject* src;
    UObject* dst;
    if (getUObjectParam( exec, 0, src ) &&
        getUObjectParam( exec, 1, dst ))
    {
        return new BLong( src->realm->has_los( *src, *dst->toplevel_owner() ) );
    }
    else
    {
        return new BLong(0);
    }
}

BObjectImp* UOExecutorModule::mf_CheckLosAt()
{
    UObject* src;
    long x;
    long y;
    long z;
    if (getUObjectParam( exec, 0, src ) &&
        getParam( 1, x ) &&
        getParam( 2, y ) &&
        getParam( 3, z ))
    {
		if(!src->realm->valid(x,y,z)) return new BError("Invalid Coordinates for realm");
        LosObj att(*src);
        LosObj tgt(static_cast<unsigned short>(x),
                   static_cast<unsigned short>(y),
                   static_cast<signed char>(z));
        return new BLong( src->realm->has_los( att, tgt ) );
    }
    else
    {
        return NULL;
    }
}

BObjectImp* UOExecutorModule::mf_DestroyItem()
{
    Item* item;
    if (getItemParam( exec, 0, item ))
    {
        if ( item->inuse() )
		{
			if ( !is_reserved_to_me(item) )
			{
				return new BError( "That item is reserved." );
			}
			else if ( item->is_gotten() )
	        {	
		        return new BError( "That item is 'gotten'." );
	        }
		}

		const ItemDesc& id = find_itemdesc( item->objtype_ );
		if (!id.destroy_script.empty())
        {
            BObjectImp* res = run_script_to_completion( id.destroy_script,
                                                        new EItemRefObjImp( item ));
            if (res->isTrue())
            {                   // destruction is okay
                BObject ob( res );
            }
            else
            {
                                // destruction isn't okay!
                return res;
            }
        }

		UpdateCharacterWeight(item);
        destroy_item( item );

        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* UOExecutorModule::mf_SetName()
{
    UObject* obj;
    const String* name_str;

    if (getUObjectParam( exec, 0, obj ) &&
        getStringParam( 1, name_str ))
    {
        obj->setname( name_str->value() );
        return new BLong(1);
    }
    else
    {
        return new BLong(0);
    }
}

BObjectImp* UOExecutorModule::mf_GetPosition()
{
    UObject* obj;
    if (getUObjectParam( exec, 0, obj ))
    {
        BStruct* arr = new BStruct;
        arr->addMember( "x", new BLong( obj->x ) );
        arr->addMember( "y", new BLong( obj->y ) );
        arr->addMember( "z", new BLong( obj->z ) );
        return arr;
    }
    else
    {
        return new BLong(0);
    }
}

void UContainer::enumerate_contents( ObjArray* arr, long flags )
{
    for( Contents::iterator itr = contents_.begin(), end = contents_.end(); itr != end; ++itr )
    {
        Item* item = GET_ITEM_PTR( itr );
		if( item ) //dave 1/1/03, wornitemscontainer can have null items!
		{
			arr->addElement( new EItemRefObjImp( item ) );
			// Austin 9-15-2006, added flag to not enumerate sub-containers.
			if ( !(flags & ENUMERATE_ROOT_ONLY) && (item->isa(CLASS_CONTAINER)) ) // FIXME check locks
			{
	            UContainer* cont = static_cast<UContainer*>(item);
		        if ( !cont->locked_ || (flags & ENUMERATE_IGNORE_LOCKED) )
			        cont->enumerate_contents( arr, flags );
			}
		}
    }
}

BObjectImp* UOExecutorModule::mf_EnumerateItemsInContainer()
{
    Item* item;
    long flags;
    if (getItemParam( exec, 0, item ))
    {
        if (exec.fparams.size() >= 2)
        {
            if (!getParam( 1, flags ))
                return new BError( "Invalid parameter type" );
        }
        else
        {
            flags = 0;
        }

        if (item->isa( UObject::CLASS_CONTAINER ))
        {
            auto_ptr<ObjArray> newarr( new ObjArray );

            static_cast<UContainer*>(item)->enumerate_contents( newarr.get(), flags );

            return newarr.release();
        }

        return NULL;
    }
    else
        return new BError( "Invalid parameter type" );
}

BObjectImp* UOExecutorModule::mf_EnumerateOnlineCharacters()
{
    auto_ptr<ObjArray> newarr( new ObjArray );

    for( Clients::const_iterator itr = clients.begin(); itr != clients.end(); ++itr )
    {
        if ((*itr)->chr != NULL)
        {
            newarr->addElement( new ECharacterRefObjImp( (*itr)->chr ) );
        }
    }

    return newarr.release();
}

BObjectImp* UOExecutorModule::mf_RegisterForSpeechEvents()
{
    UObject* center;
    long range;
    long flags = 0;
    if (getUObjectParam( exec, 0, center ) &&
        getParam( 1, range ))
    {
        if (exec.hasParams(3))
        {
            if (!getParam( 2, flags ))
                return new BError( "Invalid parameter type" );
        }
        if (!registered_for_speech_events)
        {
            register_for_speech_events( center, &uoexec, range, flags );
            registered_for_speech_events = true;
            return new BLong(1);
        }
        else
        {
            return new BError( "Already registered for speech events" );
        }
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* UOExecutorModule::mf_EnableEvents()
{
    long eventmask;
    if (getParam( 0, eventmask ))
    {
        if (eventmask & (EVID_ENTEREDAREA|EVID_LEFTAREA|EVID_SPOKE))
        {
            unsigned short range;
            if (getParam( 1, range, 0, 32 ))
            {
                if (eventmask & (EVID_SPOKE))
                    uoexec.speech_size = range;
                if (eventmask & (EVID_ENTEREDAREA|EVID_LEFTAREA))
                    uoexec.area_size = range;
            }
            else
            {
                return NULL;
            }
        }
        uoexec.eventmask |= eventmask;
        return new BLong(uoexec.eventmask);
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_DisableEvents()
{
    long eventmask;
    if (getParam( 0, eventmask ))
    {
        uoexec.eventmask &= ~eventmask;

        return new BLong(uoexec.eventmask);
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_Resurrect()
{
    Character* chr;
    long flags = 0;
    if (getCharacterParam( exec, 0, chr ))
    {
        if (!chr->dead())
            return new BError( "That is not dead" );

        if (exec.hasParams( 2 ))
        {
            if (!getParam( 1, flags ))
                return new BError( "Invalid parameter type" );
        }

        if (~flags & RESURRECT_FORCELOCATION)
        {
            // we want doors to block ghosts in this case.
            bool doors_block = ! (chr->graphic == UOBJ_GAMEMASTER ||
                                  chr->cached_settings.ignoredoors);
            int newz;
            UMulti* supporting_multi;
            Item* walkon_item;
            if (!chr->realm->walkheight( chr->x, chr->y, chr->z, &newz, &supporting_multi, &walkon_item, doors_block, chr->movemode ))
            {
                return new BError( "That location is blocked" );
            }
        }

        chr->resurrect();
        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* UOExecutorModule::mf_SystemFindObjectBySerial()
{
    long serial;
    long sysfind_flags = 0;
    if (getParam( 0, serial ))
    {
        if (exec.hasParams(2))
        {
            if (!getParam( 1, sysfind_flags ))
                return new BError( "Invalid parameter type" );
        }
        if (IsCharacter(serial))
        {
            Character* chr = system_find_mobile( serial, sysfind_flags );
            if (chr != NULL)
            {
                if (sysfind_flags & SYSFIND_SEARCH_OFFLINE_MOBILES)
                    return new EOfflineCharacterRefObjImp(chr);
                else
                    return new ECharacterRefObjImp( chr );
            }
            else
            {
                return new BError( "Character not found" );
            }
        }
        else
        {
			//dave changed this 3/8/3: objecthash (via system_find_item) will find any kind of item, so don't need system_find_multi here.
            Item* item = system_find_item( serial, sysfind_flags );

            if (item != NULL)
			{
                return item->make_ref();
            }

            return new BError( "Item not found." );
        }
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}


#include "uimport.h"
#include "gameclck.h"
void cancel_all_trades();
BObjectImp* UOExecutorModule::mf_SaveWorldState()
{
    update_gameclock();
    long flags = 0;
    if (exec.hasParams( 1 ))
    {
        if (!getParam( 0, flags ))
            return new BError( "Invalid parameter type" );
    }
    try
    {
        cancel_all_trades();

        PolClockPauser pauser;

        unsigned long dirty, clean, elapsed_ms;
        int res;
        if (flags & SAVE_INCREMENTAL)
        {
            res = save_incremental(dirty,clean,elapsed_ms);
        }
        else
        {
            res = write_data(dirty,clean,elapsed_ms);
        }
        if (res == 0)
        {
            BStruct* res = new BStruct();
            res->addMember( "DirtyObjects", new BLong( dirty ) );
            res->addMember( "CleanObjects", new BLong( clean ) );
            res->addMember( "ElapsedMilliseconds", new BLong( elapsed_ms ) );
            return res;
        }
        else
        {
            return new BError( "pol.cfg has InhibitSaves=1" );
        }
    }
    catch( std::exception& ex )
    {
        Log( "Exception during world save! (%s)\n", ex.what() );
        return new BError( "Exception during world save" );
    }
}

#include "zone.h"
#include "lightlvl.h"
#include "miscrgn.h"

BObjectImp* UOExecutorModule::mf_SetRegionLightLevel()
{
    const String* region_name_str;
    long lightlevel;
    if (! (getStringParam( 0, region_name_str ) &&
           getParam( 1, lightlevel )) )
    {
        return new BError( "Invalid Parameter type" );
    }

    if (!VALID_LIGHTLEVEL(lightlevel))
    {
        return new BError( "Light Level is out of range" );
    }

    LightRegion* lightregion = lightdef->getregion( region_name_str->value() );
    if (lightregion == NULL)
    {
        return new BError( "Light region not found" );
    }

    SetRegionLightLevel( lightregion, lightlevel );
    return new BLong(1);
}
#include "wthrtype.h"
BObjectImp* UOExecutorModule::mf_SetRegionWeatherLevel()
{
    const String* region_name_str;
    long type;
    long severity;
    long aux;
    long lightoverride;
    if (! (getStringParam( 0, region_name_str ) &&
           getParam( 1, type ) &&
           getParam( 2, severity ) &&
           getParam( 3, aux ) &&
           getParam( 4, lightoverride )) )
    {
        return new BError( "Invalid Parameter type" );
    }

    WeatherRegion* weatherregion = weatherdef->getregion( region_name_str->value() );
    if (weatherregion == NULL)
    {
        return new BError( "Weather region not found" );
    }

    SetRegionWeatherLevel( weatherregion, type, severity, aux, lightoverride );

    return new BLong(1);
}
BObjectImp* UOExecutorModule::mf_AssignRectToWeatherRegion()
{
    const String* region_name_str;
    unsigned short xwest, ynorth, xeast, ysouth;
	const String* strrealm;
	Realm* realm = find_realm(string("britannia"));

    if (! (getStringParam( 0, region_name_str ) &&
           getParam( 1, xwest ) &&
           getParam( 2, ynorth ) &&
           getParam( 3, xeast ) &&
           getParam( 4, ysouth ) &&
		   getStringParam( 5, strrealm)) )
    {
        return new BError( "Invalid Parameter type" );
    }
	realm = find_realm(strrealm->value());
	if(!realm) return new BError("Realm not found");
	if(!realm->valid(xwest,ynorth,0)) return new BError("Invalid Coordinates for realm");
	if(!realm->valid(xeast,ysouth,0)) return new BError("Invalid Coordinates for realm");

    bool res = weatherdef->assign_zones_to_region( region_name_str->data(),
                                                  xwest, ynorth,
                                                  xeast, ysouth,
												  realm );
    if (res)
        return new BLong(1);
    else
        return new BError( "Weather region not found" );
}

BObjectImp* UOExecutorModule::mf_Distance()
{
    UObject* obj1;
    UObject* obj2;
    if (getUObjectParam( exec, 0, obj1 ) &&
        getUObjectParam( exec, 1, obj2 ))
    {
        const UObject* tobj1 = obj1->toplevel_owner();
        const UObject* tobj2 = obj2->toplevel_owner();
        int xd = tobj1->x - tobj2->x;
        int yd = tobj1->y - tobj2->y;
        if (xd < 0) xd = -xd;
        if (yd < 0) yd = -yd;
        if (xd > yd)
            return new BLong(xd);
        else
            return new BLong(yd);
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* UOExecutorModule::mf_CoordinateDistance()
{
    unsigned short x1, y1, x2, y2;
	if ( !(getParam(0, x1) && getParam(1, y1)
		&& getParam(2, x2) && getParam(3, y2)) )
	{
		return new BError("Invalid parameter type");
	}
	return new BLong(pol_distance(x1, y1, x2, y2));
}

BObjectImp* UOExecutorModule::mf_GetCoordsInLine()
{
	long x1, y1, x2, y2;
	if ( !(getParam(0, x1) && getParam(1, y1)
		&& getParam(2, x2) && getParam(3, y2)) )
	{
		return new BError("Invalid parameter type");
	}
	else if ( x1 == x2 && y1 == y2 )
	{	// Same exact coordinates ... just give them the coordinate back!
		ObjArray* coords = new ObjArray;
		BStruct* point = new BStruct;
		point->addMember("x", new BLong(x1));
		point->addMember("y", new BLong(y1));
		coords->addElement(point);
		return coords;
	}

	double dx = abs(x2-x1)+0.5;
	double dy = abs(y2-y1)+0.5;
	long vx = 0, vy = 0;

	if ( x2 > x1 )
		vx = 1;
	else if ( x2 < x1 )
		vx = -1;
	if ( y2 > y1 )
		vy = 1;
	else if ( y2 < y1 )
		vy = -1;
	
	ObjArray* coords = new ObjArray;
	if ( dx >= dy )
	{
		dy = dy / dx;
		
		for ( long c = 0; c <= dx; c++ )
		{
			long point_x = x1 + (c * vx);
			
			double float_y = double(c) * double(vy) * dy;
			if ( float_y - floor(float_y) >= 0.5 )
				float_y = ceil(float_y);
			long point_y = long(float_y) + y1;
			
			BStruct* point = new BStruct;
			point->addMember("x", new BLong(point_x));
			point->addMember("y", new BLong(point_y));
			coords->addElement(point);
		}
	}
	else
	{
		dx = dx / dy;
		for ( long c = 0; c <= dy; c++ )
		{
			long point_y = y1 + (c * vy);
			
			double float_x = double(c) * double(vx) * dx;
			if ( float_x - floor(float_x) >= 0.5 )
				float_x = ceil(float_x);
			long point_x = long(float_x) + x1;
			
			BStruct* point = new BStruct;
			point->addMember("x", new BLong(point_x));
			point->addMember("y", new BLong(point_y));
			coords->addElement(point);
		}
	}
	return coords;
}

BObjectImp* UOExecutorModule::mf_GetFacing()
{
	unsigned short from_x, from_y, to_x, to_y;
	if ( !(getParam(0, from_x) && getParam(1, from_y)
		&& getParam(2, to_x) && getParam(3, to_y)) )
	{
		return new BError("Invalid parameter type");
	}

	double x = to_x - from_x;
	double y = to_y - from_y;
	double pi = acos(double(-1));
	double r = sqrt(x*x + y*y);
	
	double angle = ((acos(x/r) * 180.0) / pi);
    
	double y_check = ((asin(y/r) * 180.0) / pi);
	if ( y_check < 0 )
	{
		angle = 360 - angle;
	}

	unsigned short facing = ((short(angle/40)+10)%8);

	return new BLong(facing);
}

void true_extricate( Item* item )
{
    ConstForEach( clients, send_remove_object_if_inrange, item );
    if (item->container != NULL)
    {
        item->extricate();
    }
    else
    {
        remove_item_from_world( item );
    }
}

BObjectImp* UOExecutorModule::mf_MoveItemToContainer()
{
    Item* item;
    Item* cont_item;
	long px;
	long py;
    if ( !(getItemParam( exec, 0, item ) &&
           getItemParam( exec, 1, cont_item ) &&
           getParam( 2, px, -1, 65535 ) &&
           getParam( 3, py, -1, 65535 )) )
    {
        return new BError( "Invalid parameter type" );
    }

	ItemRef itemref(item); //dave 1/28/3 prevent item from being destroyed before function ends
    if (!item->movable())
    {
        Character* chr = controller_.get();
        if (chr == NULL || !chr->can_move( item ))
            return new BError( "That is immobile" );
    }
    if (item->inuse() && !is_reserved_to_me(item))
    {
        return new BError( "That item is being used." );
    }


    if (!cont_item->isa( UObject::CLASS_CONTAINER ))
    {
        return new BError( "Non-container selected as target" );
    }
    UContainer* cont = static_cast<UContainer*>(cont_item);

    if (cont->serial == item->serial)
    {
        return new BError( "Can't put a container into itself" );
    }
    if (is_a_parent( cont, item->serial ))
    {
        return new BError( "Can't put a container into an item in itself" );
    }
    if (!cont->can_add( *item ))
    {
        return new BError( "Container is too full to add that" );
    }
	//DAVE added this 12/04, call can/onInsert & can/onRemove scripts for this container
	Character* chr_owner = cont->GetCharacterOwner();
	if(chr_owner == NULL)
		if(controller_.get() != NULL)
			chr_owner = controller_.get();

	//daved changed order 1/26/3 check canX scripts before onX scripts.
	UContainer* oldcont = item->container;

	if( (oldcont != NULL) &&
		(!oldcont->check_can_remove_script( chr_owner, item, UContainer::MT_CORE_MOVED )) )
		return new BError( "Could not remove item from its container." );
	if(item->orphan()) //dave added 1/28/3, item might be destroyed in RTC script
	{
		return new BError( "Item was destroyed in CanRemove script" );
	}

	if(!cont->can_insert_add_item( chr_owner, UContainer::MT_CORE_MOVED, item ))
		return new BError( "Could not insert item into container." );
	if(item->orphan()) //dave added 1/28/3, item might be destroyed in RTC script
	{
		return new BError( "Item was destroyed in CanInsert Script" );
	}

    if (!item->check_unequiptest_scripts() || !item->check_unequip_script())
        return new BError( "Item cannot be unequipped" );
	if(item->orphan()) //dave added 1/28/3, item might be destroyed in RTC script
	{
		return new BError( "Item was destroyed in Equip Script" );
	}

	if(oldcont != NULL)
	{
		oldcont->on_remove( chr_owner, item, UContainer::MT_CORE_MOVED );
		if(item->orphan()) //dave added 1/28/3, item might be destroyed in RTC script
		{
			return new BError( "Item was destroyed in OnRemove script" );
		}
	}

	short x = static_cast<short>(px);
	short y = static_cast<short>(py);
    if (/*x < 0 || y < 0 ||*/ !cont->is_legal_posn( item, x, y ))
    {
        u16 tx, ty;
        cont->get_random_location( &tx, &ty );
        x = tx;
        y = ty;
    }

    true_extricate( item );

    item->x = x;
    item->y = y;
    item->z = 0;
    cont->add( item );
    update_item_to_inrange( item );
	//DAVE added this 11/17: if in a Character's pack, update weight.
	UpdateCharacterWeight(item);

    cont->on_insert_add_item( chr_owner, UContainer::MT_CORE_MOVED, item );
	if(item->orphan()) //dave added 1/28/3, item might be destroyed in RTC script
	{
		return new BError( "Item was destroyed in OnInsert script" );
	}

    return new BLong(1);
}

BObjectImp* place_item_in_secure_trade_container( Client* client, Item* item );
BObjectImp* UOExecutorModule::mf_MoveItemToSecureTradeWin()
{
    Item* item;
    Character* chr;
    if ( !(getItemParam( exec, 0, item ) &&
           getCharacterParam( exec, 1, chr )) )
    {
        return new BError( "Invalid parameter type" );
    }
    
    ItemRef itemref(item); //dave 1/28/3 prevent item from being destroyed before function ends
    if (!item->movable())
    {
        Character* chr = controller_.get();
        if (chr == NULL || !chr->can_move( item ))
            return new BError( "That is immobile" );
    }
    if (item->inuse() && !is_reserved_to_me(item))
    {
        return new BError( "That item is being used." );
    }

	//daved changed order 1/26/3 check canX scripts before onX scripts.
	UContainer* oldcont = item->container;

	//DAVE added this 12/04, call can/onInsert & can/onRemove scripts for this container
	Character* chr_owner = oldcont->GetCharacterOwner();
	if(chr_owner == NULL)
		if(controller_.get() != NULL)
			chr_owner = controller_.get();

	if( (oldcont != NULL) &&
		(!oldcont->check_can_remove_script( chr_owner, item, UContainer::MT_CORE_MOVED )) )
		return new BError( "Could not remove item from its container." );
	if(item->orphan()) //dave added 1/28/3, item might be destroyed in RTC script
	{
		return new BError( "Item was destroyed in CanRemove script" );
	}

    if (!item->check_unequiptest_scripts() || !item->check_unequip_script())
        return new BError( "Item cannot be unequipped" );
	if(item->orphan()) //dave added 1/28/3, item might be destroyed in RTC script
	{
		return new BError( "Item was destroyed in Equip Script" );
	}

	if(oldcont != NULL)
	{
		oldcont->on_remove( chr_owner, item, UContainer::MT_CORE_MOVED );
		if(item->orphan()) //dave added 1/28/3, item might be destroyed in RTC script
		{
			return new BError( "Item was destroyed in OnRemove script" );
		}
	}
    
    true_extricate( item );
    
    return place_item_in_secure_trade_container( chr->client, item );
}

BObjectImp* UOExecutorModule::mf_EquipItem()
{
    Character* chr;
    Item* item;
    if (getCharacterParam( exec, 0, chr ) &&
        getItemParam( exec, 1, item ))
    {
		ItemRef itemref(item); //dave 1/28/3 prevent item from being destroyed before function ends
        if (!item->movable())
        {
            Character* chr = controller_.get();
            if (chr == NULL || !chr->can_move( item ))
                return new BError( "That is immobile" );
        }

        if (item->inuse() && !is_reserved_to_me(item))
        {
            return new BError( "That item is being used." );
        }

        if (item->objtype_ != EXTOBJ_MOUNT)
        {
            item->layer = tilelayer( item->graphic );
        }

        if (!chr->equippable( item ) ||
            !item->check_equiptest_scripts( chr ))
        {
            return new BError( "That item is not equippable by that character" );
        }
		if(item->orphan()) //dave added 1/28/3, item might be destroyed in RTC script
		{
			return new BError( "Item was destroyed in EquipTest script" );
		}

        if (item->has_equip_script())
        {
            BObjectImp* res = item->run_equip_script( chr, false );
            if (!res->isTrue())
                return res;
            else
                BObject obj( res );
        }


        true_extricate( item );

        // at this point, 'item' is free - doesn't belong to the world, or a container.
        chr->equip( item );
	    send_wornitem_to_inrange( chr, item );

        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* UOExecutorModule::mf_RestartScript()
{
    Character* chr;
    if (getCharacterParam( exec, 0, chr ))
    {
        if (chr->isa( UObject::CLASS_NPC ))
        {
            NPC* npc = static_cast<NPC*>(chr);
            npc->restart_script();
            return new BLong(1);
        }
        else
        {
            return new BError( "RestartScript only operates on NPCs" );
        }
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

#include "resource.h"

BObjectImp* UOExecutorModule::mf_GetHarvestDifficulty()
{
    const String* resource;
    xcoord x;
    ycoord y;
    unsigned short tiletype;
	const String* strrealm;
	Realm* realm = find_realm(string("britannia"));

    if (getStringParam( 0, resource ) &&
        getParam( 1, x ) &&
        getParam( 2, y ) &&
        getParam( 3, tiletype ) &&
		getStringParam( 4, strrealm) )
    {
		realm = find_realm(strrealm->value());
		if(!realm) return new BError("Realm not found");
		if(!realm->valid(x,y,0)) return new BError("Invalid Coordinates for realm");

        return get_harvest_difficulty( resource->data(), x, y, realm, tiletype );
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_HarvestResource()
{
    xcoord x;
    ycoord y;
    const String* resource;
    long b;
    long n;
	const String* strrealm;
	Realm* realm = find_realm(string("britannia"));

    if (getStringParam( 0, resource ) &&
        getParam( 1, x ) &&
        getParam( 2, y ) &&
        getParam( 3, b ) &&
        getParam( 4, n ) &&
		getStringParam( 5, strrealm) )
    {
		realm = find_realm(strrealm->value());
		if(!realm) return new BError("Realm not found");
		if(!realm->valid(x,y,0)) return new BError("Invalid Coordinates for realm");

		if(b <= 0)
			return new BError("b must be >= 0");
        return harvest_resource( resource->data(), x, y, realm, b, n );
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_GetRegionName(/* objref */)
{
	UObject* obj;
	JusticeRegion* justice_region;

	if (getUObjectParam( exec, 0, obj ))
	{
		if (obj->isa(UObject::CLASS_ITEM))
			obj = obj->toplevel_owner();

		if (obj->isa(UObject::CLASS_CHARACTER))
		{
			Character* chr = static_cast<Character*>(obj);

			if (chr->logged_in)
				justice_region = chr->client->gd->justice_region;
			else
				justice_region = justicedef->getregion( chr->x, chr->y, chr->realm );
		}
		else
			justice_region = justicedef->getregion( obj->x, obj->y, obj->realm );

		if (justice_region == NULL)
			return new BError("No Region defined at this Location");
		else
			return new String(justice_region->region_name());
	}
	else
		return new BError("Invalid parameter");
}

BObjectImp* UOExecutorModule::mf_GetRegionNameAtLocation(/* x, y, realm */)
{
	long x, y;
	const String* strrealm;
	Realm* realm;

	if (getParam( 0, x ) &&
		getParam( 1, y ) &&
		getStringParam( 2, strrealm ))
	{
		realm = find_realm(strrealm->value());
		if (!realm)
			return new BError("Realm not found");
		if (!realm->valid(x,y,0))
			return new BError("Invalid Coordinates for realm");

		JusticeRegion* justice_region = justicedef->getregion( x, y, realm );
		if (justice_region == NULL)
			return new BError("No Region defined at this Location");
		else
			return new String(justice_region->region_name());
	}
	else
		return new BError("Invalid parameter");
}

BObjectImp* UOExecutorModule::mf_GetRegionString()
{
    const String* resource;
    unsigned short x,y;
    const String* propname;
	const String* strrealm;
	Realm* realm = find_realm(string("britannia"));

    if (getStringParam( 0, resource ) &&
        getParam( 1, x ) &&
        getParam( 2, y ) &&
        getStringParam( 3, propname ) &&
		getStringParam( 4, strrealm ))
    {
		realm = find_realm(strrealm->value());
		if(!realm) return new BError("Realm not found");
		if(!realm->valid(x,y,0)) return new BError("Invalid Coordinates for realm");

        return get_region_string( resource->data(), x, y, realm, propname->value() );
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* equip_from_template( Character* chr, const char* template_name );
BObjectImp* UOExecutorModule::mf_EquipFromTemplate()
{
    Character* chr;
    const String* template_name;
    if (getCharacterParam( exec, 0, chr ) &&
        getStringParam( 1, template_name ))
    {
        return equip_from_template( chr, template_name->data() );
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_GrantPrivilege()
{
    Character* chr;
    const String* privstr;
    if (getCharacterParam( exec, 0, chr ) &&
        getStringParam( 1, privstr ))
    {
        chr->grant_privilege( privstr->data() );
        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_RevokePrivilege()
{
    Character* chr;
    const String* privstr;
    if (getCharacterParam( exec, 0, chr ) &&
        getStringParam( 1, privstr ))
    {
        chr->revoke_privilege( privstr->data() );
        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_ReadGameClock()
{
    return new BLong( read_gameclock() );
}

unsigned char decode_xdigit( unsigned char ch )
{
    if (ch >= '0' && ch <= '9')
        ch -= '0';
    else if (ch >= 'A' && ch <= 'F')
        ch = ch - 'A' + 0xa;
    else if (ch >= 'a' && ch <= 'f')
        ch = ch - 'a' + 0xa;

    return ch;
}

BObjectImp* UOExecutorModule::mf_SendPacket()
{
    Character* chr;
    const String* str;
    if (getCharacterParam( exec, 0, chr ) &&
        getStringParam( 1, str ))
    {
        static unsigned char buffer[ 2000 ];
        const char*s = str->data();
        int buflen = 0;
        while (buflen < 2000 && isxdigit( s[0] ) && isxdigit( s[1] ))
        {
            unsigned char ch;
            ch =  (decode_xdigit( s[0] ) << 4) | decode_xdigit( s[1] );
            buffer[ buflen++ ] = ch;
            s += 2;
        }
        if (chr->has_active_client())
        {
            //printf( "SendPacket() data: %d bytes\n", buflen );
            //fdump( stdout, buffer, buflen );
            chr->client->transmit( buffer, buflen );
            return new BLong(1);
        }
        else
        {
            return new BError( "No client attached" );
        }
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* UOExecutorModule::mf_SendQuestArrow()
{
	Character* chr;
    long x,y;
    if (getCharacterParam( exec, 0, chr ) &&
		getParam( 1, x, -1, 1000000 ) &&
        getParam( 2, y, -1, 1000000 ))  //max vaues checked below
    {
		MSGOUT_BA_QUEST_ARROW msg;
		msg.msgtype = MSGOUT_BA_QUEST_ARROW_ID;
		if ( x == -1 && y == -1 )
		{
			msg.active = MSGOUT_BA_QUEST_ARROW::ARROW_OFF;
            msg.x_tgt = 0;
			msg.y_tgt = 0;
		}
		else
		{
			if(!chr->realm->valid(x,y,0)) return new BError("Invalid Coordinates for Realm");
			msg.active = MSGOUT_BA_QUEST_ARROW::ARROW_ON;
			msg.x_tgt = ctBEu16(static_cast<unsigned short>(x & 0xFFFF));
			msg.y_tgt = ctBEu16(static_cast<unsigned short>(y & 0xFFFF));
		}
		if (!chr->has_active_client())
            return new BError( "No client attached" );

		chr->client->transmit(&msg, sizeof msg);
        return new BLong( 1 );
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_ConsumeReagents()
{
    Character* chr;
    long spellid;
    if (getCharacterParam( exec, 0, chr ) &&
        getParam( 1, spellid ))
    {
        if (!VALID_SPELL_ID(spellid))
        {
            return new BError( "Spell ID out of range" );
        }
        USpell* spell = spells2[ spellid ];
        if (spell == NULL)
        {
            return new BError( "No such spell" );
        }

        return new BLong( spell->consume_reagents( chr ) ? 1 : 0 );
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_StartSpellEffect()
{
    Character* chr;
    long spellid;
    if (getCharacterParam( exec, 0, chr ) &&
        getParam( 1, spellid ))
    {
        if (!VALID_SPELL_ID(spellid))
        {
            return new BError( "Spell ID out of range" );
        }
        USpell* spell = spells2[ spellid ];
        if (spell == NULL)
        {
            return new BError( "No such spell" );
        }

        spell->cast( chr );
        return new BLong( 1 );
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}
BObjectImp* UOExecutorModule::mf_GetSpellDifficulty()
{
    long spellid;
    if (getParam( 0, spellid ))
    {
        if (!VALID_SPELL_ID(spellid))
        {
            return new BError( "Spell ID out of range" );
        }
        USpell* spell = spells2[ spellid ];
        if (spell == NULL)
        {
            return new BError( "No such spell" );
        }

        return new BLong( spell->difficulty() );
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}
BObjectImp* UOExecutorModule::mf_SpeakPowerWords()
{
    Character* chr;
    long spellid;
    if (getCharacterParam( exec, 0, chr ) &&
        getParam( 1, spellid ))
    {
        if (!VALID_SPELL_ID(spellid))
        {
            return new BError( "Spell ID out of range" );
        }
        USpell* spell = spells2[ spellid ];
        if (spell == NULL)
        {
            return new BError( "No such spell" );
        }

        spell->speak_power_words( chr );

        return new BLong( 1 );
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_ListEquippedItems()
{
    Character* chr;
    if (getCharacterParam( exec, 0, chr ))
    {
        ObjArray* arr = new ObjArray;
        for( int layer = LAYER_EQUIP__LOWEST; layer <= LAYER_EQUIP__HIGHEST; ++layer )
	    {
		    Item* item = chr->wornitem( layer );
		    if (item != NULL)
            {
                arr->addElement( new EItemRefObjImp( item ) );
            }
	    }
	    return arr;
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_GetEquipmentByLayer()
{
    Character* chr;
    long layer;
    if (getCharacterParam( exec, 0, chr ) &&
        getParam( 1, layer ))
    {
        if (layer < LOWEST_LAYER || layer > HIGHEST_LAYER)
        {
            return new BError( "Invalid layer" );
        }

        Item* item = chr->wornitem( layer );
        if (item == NULL)
        {
            return new BError( "Nothing equipped on that layer." );
        }
        else
        {
            return new EItemRefObjImp( item );
        }
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_DisconnectClient()
{
    Character* chr;
    if (getCharacterParam( exec, 0, chr ))
    {
        if (chr->has_active_client())
        {
            chr->client->disconnect = 1;
            return new BLong(1);
        }
        else
        {
            return new BError( "No client is attached" );
        }
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_GetMapInfo()
{
    xcoord x;
    ycoord y;
	const String* strrealm;
	Realm* realm = find_realm(string("britannia"));

        // note that this uses WORLD_MAX_X, not WORLD_X,
        // because we can't read the outermost edge of the map
    if (getParam( 0, x ) && // FIXME realm size
        getParam( 1, y ) &&
		getStringParam(2, strrealm) ) // FIXME realm size
    {
		realm = find_realm(strrealm->value());
		if(!realm) return new BError("Realm not found");
		if(!realm->valid(x,y,0)) return new BError("Invalid Coordinates for realm");

        MAPTILE_CELL cell = realm->getmaptile( static_cast<unsigned short>(x), static_cast<unsigned short>(y) );

        BStruct* result = new BStruct;
        result->addMember( "z", new BLong( cell.z ) );
        result->addMember( "landtile", new BLong( cell.landtile ) );

        return result;
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}
BObjectImp* UOExecutorModule::mf_GetWorldHeight()
{
    unsigned x, y;
	const String* strrealm;
	Realm* realm = find_realm(string("britannia"));
    if (getParam( 0, x ) &&
        getParam( 1, y ) &&
		getStringParam( 2, strrealm) )
    {
		realm = find_realm(strrealm->value());
		if(!realm) return new BError("Realm not found");
        if(!realm->valid(x,y,0)) return new BError("Invalid Coordinates for Realm");

        int z = 255;
        if (realm->lowest_standheight( x, y, &z ))
            return new BLong(z);
        else
            return new BError( "Nowhere" );
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_GetObjtypeByName()
{
    const String* namestr;
    if (getStringParam( 0, namestr ))
    {
        unsigned short objtype = get_objtype_byname( namestr->data() );
        if (objtype != 0)
            return new BLong( objtype );
        else
            return new BError( "No objtype by that name" );
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_SendEvent()
{
    Character* chr;
    if (getCharacterParam( exec, 0, chr ))
    {
        BObjectImp* event = exec.getParamImp( 1 );
        if (event != NULL)
        {
            if (chr->isa( UObject::CLASS_NPC ))
            {
                NPC* npc = static_cast<NPC*>(chr);
                // event->add_ref(); // UNTESTED
                return npc->send_event( event->copy() );
            }
            else
            {
                return new BError( "That mobile is not an NPC" );
            }
        }
        else
        {
            return new BError( "Huh?  Not enough parameters" );
        }
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}
#include "house.h"
BObjectImp* UOExecutorModule::mf_DestroyMulti()
{
    UMulti* multi;
    if (getMultiParam( exec, 0, multi ))
    {
        UBoat* boat = multi->as_boat();
        if (boat != NULL)
        {
            return destroy_boat( boat );
        }
        UHouse* house = multi->as_house();
        if (house != NULL)
        {
            return destroy_house( house );
        }
        return new BError( "WTF!? Don't know what kind of multi that is!" );
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}


BObjectImp* UOExecutorModule::mf_GetMultiDimensions()
{
	unsigned int graphic;
	if (getParam( 0, graphic ))
	{
		if(!MultiDefByGraphicExists(graphic))
			return new BError("Multi Graphic not found");

		const MultiDef& md = *MultiDefByGraphic(graphic);
		BStruct* ret = new BStruct;
		ret->addMember( "xmin", new BLong( md.minrx ) );
		ret->addMember( "xmax", new BLong( md.maxrx ) );
		ret->addMember( "ymin", new BLong( md.minry ) );
		ret->addMember( "ymax", new BLong( md.maxry ) );
		return ret;
	}
	else
		return new BError("Invalid parameter");

}

BObjectImp* UOExecutorModule::mf_GameStat()
{
    const String* stat_str;
    if (getStringParam( 0, stat_str ))
    {
        const char* stat = stat_str->data();
        if (stricmp( stat, "itemcount" ) == 0)
            return new BLong( get_toplevel_item_count() );
        else if (stricmp( stat, "mobilecount" ) == 0)
            return new BLong( get_mobile_count() );
        else
            return new BError( "Unknown statistic name" );
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* UOExecutorModule::mf_SetScriptController()
{
    Character* old_controller = controller_.get();
    BObjectImp* param0 = getParamImp( 0 );
    bool handled = false;

    if (param0->isa( BObjectImp::OTLong ))
    {
        BLong* lng = static_cast<BLong*>(param0);
        if (lng->value() == 0)
        {
            controller_.clear();
            handled = true;
        }
    }

    if (!handled)
    {
        Character* chr;
        if (getCharacterParam( exec, 0, chr ))
            controller_.set(chr);
        else
            controller_.clear();
    }

    if (old_controller)
        return new ECharacterRefObjImp( old_controller );
    else
        return new BLong(0);
}


/*
BObjectImp* UOExecutorModule::mf_AssignMultiComponent()
{
    UMulti* multi;
    Item* item;
    if (getMultiParam( *this, 0, multi ) &&
        getItemParan( 1, item ))
    {
        UHouse* house = multi->as_house();
        if (house != NULL)
        {
            house->add_component( item );
            return new BLong(1);
        }
        else
        {
            return new BError( "AssignMultiComponent only functions on houses" );
        }
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}
*/

BObjectImp* UOExecutorModule::mf_GetStandingHeight()
{
    long x, y, z;
	const String* strrealm;
	Realm* realm = find_realm(string("britannia"));
    if (getParam( 0, x ) &&
        getParam( 1, y ) &&
        getParam( 2, z ) &&
		getStringParam( 3, strrealm) )
    {
		realm = find_realm(strrealm->value());
		if(!realm) return new BError("Realm not found");
		if(!realm->valid(x,y,z)) return new BError("Coordinates Invalid for Realm");
        int newz;
        UMulti* multi;
        Item* walkon;
        if (realm->walkheight( x, y, z, &newz, &multi, &walkon, true, MOVEMODE_LAND ))
        {
            BStruct* arr = new BStruct;
            arr->addMember( "z", new BLong( newz ) );
            if (multi != NULL)
                arr->addMember( "multi", new EMultiRefObjImp( multi ) );
            return arr;
        }
        else
        {
            return new BError( "Can't stand there" );
        }
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* UOExecutorModule::mf_GetStandingLayers(/* x, y, flags, realm */)
{
    long x, y, flags;
    const String* strrealm;
    Realm* realm;
    
    if (getParam( 0, x ) &&
        getParam( 1, y ) &&
        getParam( 2, flags ) &&
        getStringParam( 3, strrealm) )
    {
        realm = find_realm(strrealm->value());
        if (!realm)
            return new BError("Realm not found");
        
        if (!realm->valid(x, y, 0))
            return new BError("Coordinates Invalid for Realm");
        
        auto_ptr<ObjArray> newarr( new ObjArray );

        MapShapeList mlist;
        realm->readmultis( mlist, x, y, flags );
        realm->getmapshapes( mlist, x, y, flags );

        for( unsigned i = 0; i < mlist.size(); ++i )
        {
            BStruct* arr = new BStruct;
            
            if ( mlist[i].flags & (FLAG::MOVELAND | FLAG::MOVESEA) )
                arr->addMember( "z", new BLong( mlist[i].z + 1 ) );
            else
                arr->addMember( "z", new BLong( mlist[i].z ) );
            
            arr->addMember( "height", new BLong( mlist[i].height ) );
            arr->addMember( "flags", new BLong( mlist[i].flags ) );
            newarr->addElement( arr );
        }
        
        return newarr.release();
    }
    else
        return new BError( "Invalid parameter type" );
}

BObjectImp* UOExecutorModule::mf_ReserveItem()
{
    Item* item;
    if (getItemParam( exec, 0, item ))
    {
        if (item->inuse())
        {
            if (is_reserved_to_me( item ))
                return new BLong( 1 );
            else
                return new BError( "That item is already being used." );
        }
        item->inuse( true );
        reserved_items_.push_back( ItemRef(item) );
        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

BObjectImp* UOExecutorModule::mf_ReleaseItem()
{
    Item* item;
    if (getItemParam( exec, 0, item ))
    {
        if (item->inuse())
        {
            for( unsigned i = 0; i < reserved_items_.size(); ++i )
            {
                if (reserved_items_[i].get() == item)
                {
                    item->inuse( false );
                    reserved_items_[i] = reserved_items_.back();
                    reserved_items_.pop_back();
                    return new BLong(1);
                }
            }
            return new BError( "That item is not reserved by this script." );
        }
        else
        {
            return new BError( "That item is not reserved." );
        }
    }
    else
    {
        return new BError( "Invalid parameter" );
    }
}

void send_skillmsg( Client *client, const Character *chr );

BObjectImp* UOExecutorModule::mf_SendSkillWindow()
{
    Character *towhom, *forwhom;
    if (getCharacterParam( exec, 0, towhom ) &&
        getCharacterParam( exec, 1, forwhom ))
    {
        if (towhom->has_active_client())
        {
            send_skillmsg( towhom->client, forwhom );
            return new BLong(1);
        }
        else
        {
            return new BError( "No client attached" );
        }
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

void send_paperdoll( Client *client, Character *chr );

BObjectImp* UOExecutorModule::mf_OpenPaperdoll()
{
    Character *towhom, *forwhom;
    if (getCharacterParam( exec, 0, towhom ) &&
        getCharacterParam( exec, 1, forwhom ))
    {
        if (towhom->has_active_client())
        {
            send_paperdoll( towhom->client, forwhom );
            return new BLong(1);
        }
        else
        {
            return new BError( "No client attached" );
        }
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

// TODO: a FindSubstance call that does the 'find' portion below, but returns an
// array of the stacks.  All returned items would be marked 'inuse'.
// Then, make ConsumeSubstance able to use such an array of owned items.

BObjectImp* UOExecutorModule::mf_ConsumeSubstance()
{
    Item* cont_item;
    unsigned short objtype;
    long amount;
    if (getItemParam( exec, 0, cont_item ) &&
        getObjtypeParam( exec, 1, objtype ) &&
        getParam( 2, amount ))
    {
        if (!cont_item->isa( UObject::CLASS_CONTAINER ))
            return new BError( "That is not a container" );
        if (amount < 0)
            return new BError( "Amount cannot be negative" );

        UContainer* cont = static_cast<UContainer*>(cont_item);
        long amthave = cont->find_sumof_objtype_noninuse(objtype);
        if (amthave < amount)
            return new BError( "Not enough of that substance in container" );

        cont->consume_sumof_objtype_noninuse( objtype, amount );

        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

bool UOExecutorModule::is_reserved_to_me( Item* item )
{
    for( unsigned i = 0; i < reserved_items_.size(); ++i )
    {
        if (reserved_items_[i].get() == item)
            return true;
    }
    return false;
}

BObjectImp* UOExecutorModule::mf_Shutdown()
{
    exit_signalled = 1;
#ifndef _WIN32
    // the catch_signals_thread (actually main) sits with sigwait(),
    // so it won't wake up except by being signalled.
    signal_catch_thread();
#endif
    return new BLong(1);
}

string get_textcmd_help( Character* chr, const char* cmd );
BObjectImp* UOExecutorModule::mf_GetCommandHelp()
{
    Character* chr;
    const String* cmd;
    if (getCharacterParam( exec, 0, chr ) &&
        getStringParam( 1, cmd ))
    {
        string help = get_textcmd_help( chr, cmd->value().c_str() );
        if (!help.empty())
        {
            return new String( help );
        }
        else
        {
            return new BError( "No help for that command found" );
        }
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

void send_tip( Client* client, const std::string& tiptext );
BObjectImp* UOExecutorModule::mf_SendStringAsTipWindow()
{
    Character* chr;
    const String* str;
    if (getCharacterParam( exec, 0, chr ) &&
        getStringParam( 1, str ))
    {
        if (chr->has_active_client())
        {
            send_tip( chr->client, str->value() );
            return new BLong(1);
        }
        else
        {
            return new BError( "No client attached" );
        }
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }

}

BObjectImp* UOExecutorModule::mf_ListItemsNearLocationWithFlag(/* x, y, z, range, flags, realm */) //DAVE
{
    long x, y, z, range, flags;
	const String* strrealm;
	Realm* realm;

    if (getParam( 0, x ) &&
        getParam( 1, y ) &&
        getParam( 2, z ) &&
        getParam( 3, range ) &&
        getParam( 4, flags ) &&
		getStringParam( 5, strrealm) )
    {
		realm = find_realm(strrealm->value());
		if (!realm)
            return new BError("Realm not found");

        if (z == LIST_IGNORE_Z)
        {
            if (!realm->valid(x, y, 0))
                return new BError("Invalid Coordinates for realm");
        }
        else
        {
            if (!realm->valid(x, y, z))
                return new BError("Invalid Coordinates for realm");
        }

        auto_ptr<ObjArray> newarr( new ObjArray );

        unsigned short wxL, wyL, wxH, wyH;
        zone_convert_clip( x - range, y - range, realm, wxL, wyL );
        zone_convert_clip( x + range, y + range, realm, wxH, wyH );

        for( unsigned short wx = wxL; wx <= wxH; ++wx )
        {
            for( unsigned short wy = wyL; wy <= wyH; ++wy )
            {
                ZoneItems& witem = realm->zone[wx][wy].items;
                for( ZoneItems::iterator itr = witem.begin(), end = witem.end(); itr != end; ++itr )
                {
                    Item* item = *itr;

                    if( (tile_uoflags(item->graphic) & flags) )
					{
                        if ((abs(item->x - x) <= range) && (abs(item->y - y) <= range))
                            if ((z == LIST_IGNORE_Z) || (abs(item->z - z) < CONST_DEFAULT_ZRANGE))
                                newarr->addElement( new EItemRefObjImp( item ) );
                    }
                }
            }
        }
        return newarr.release();
    }

    return new BError( "Invalid parameter" );
}

BObjectImp* UOExecutorModule::mf_ListStaticsAtLocation(/* x, y, z, flags, realm */)
{
    long x, y, z, flags;
    const String* strrealm;
    Realm* realm;
    
    if (getParam( 0, x ) &&
        getParam( 1, y ) &&
        getParam( 2, z ) &&
        getParam( 3, flags ) &&
        getStringParam( 4, strrealm) )
    {
        realm = find_realm(strrealm->value());
        if (!realm)
            return new BError("Realm not found");

        if (z == LIST_IGNORE_Z)
        {
            if (!realm->valid(x, y, 0))
                return new BError("Invalid Coordinates for realm");
        }
        else
        {
            if (!realm->valid(x, y, z))
                return new BError("Invalid Coordinates for realm");
        }

        auto_ptr<ObjArray> newarr( new ObjArray );

        if (!( flags & ITEMS_IGNORE_STATICS ))
        {
            StaticEntryList slist;
            realm->getstatics( slist, x, y );

            for( unsigned i = 0; i < slist.size(); ++i )
            {
                if ((z == LIST_IGNORE_Z) || (slist[i].z == z))
                {
                    BStruct* arr = new BStruct;
                    arr->addMember( "x", new BLong( x ) );
                    arr->addMember( "y", new BLong( y ) );
                    arr->addMember( "z", new BLong( slist[i].z ) );
                    arr->addMember( "objtype", new BLong( slist[i].objtype ) );
                    newarr->addElement( arr );
                }
            }
        }

        if (!( flags & ITEMS_IGNORE_MULTIS ))
        {
            StaticList mlist;
            realm->readmultis( mlist, x, y );

            for( unsigned i = 0; i < mlist.size(); ++i )
            {
                if ((z == LIST_IGNORE_Z) || (mlist[i].z == z))
                {
                    BStruct* arr = new BStruct;
                    arr->addMember( "x", new BLong( x ) );
                    arr->addMember( "y", new BLong( y ) );
                    arr->addMember( "z", new BLong( mlist[i].z ) );
                    arr->addMember( "objtype", new BLong( mlist[i].graphic ) );
                    newarr->addElement( arr );
                }
            }
        }

        return newarr.release();
    }
    else
        return new BError( "Invalid parameter" );
}

BObjectImp* UOExecutorModule::mf_ListStaticsNearLocation(/* x, y, z, range, flags, realm */)
{
    long x, y, z, range, flags;
    const String* strrealm;
    Realm* realm;
    
    if (getParam( 0, x ) &&
        getParam( 1, y ) &&
        getParam( 2, z ) &&
        getParam( 3, range ) &&
        getParam( 4, flags ) &&
        getStringParam( 5, strrealm) )
    {
        realm = find_realm(strrealm->value());
        if (!realm)
            return new BError("Realm not found");

        if (z == LIST_IGNORE_Z)
        {
            if (!realm->valid(x, y, 0))
                return new BError("Invalid Coordinates for realm");
        }
        else
        {
            if (!realm->valid(x, y, z))
                return new BError("Invalid Coordinates for realm");
        }

        auto_ptr<ObjArray> newarr( new ObjArray );
        
        unsigned int wxL, wyL, wxH, wyH;
        wxL = x - range;
        if (wxL < 0)
            wxL = 0;
        wyL = y - range;
        if (wyL < 0)
            wyL = 0;
        wxH = x + range;
        if (wxH > realm->width()-1)
            wxH = realm->width()-1;
        wyH = y + range;
        if (wyH > realm->height()-1)
            wyH = realm->height()-1;
        
        for( unsigned int wx = wxL; wx <= wxH; ++wx )
        {
            for( unsigned int wy = wyL; wy <= wyH; ++wy )
            {
                if (!( flags & ITEMS_IGNORE_STATICS ))
                {
                    StaticEntryList slist;
                    realm->getstatics( slist, wx, wy );

                    for( unsigned i = 0; i < slist.size(); ++i )
                    {
                        if ((z == LIST_IGNORE_Z) || (abs(slist[i].z - z) < CONST_DEFAULT_ZRANGE))
                        {
                            BStruct* arr = new BStruct;
                            arr->addMember( "x", new BLong( wx ) );
                            arr->addMember( "y", new BLong( wy ) );
                            arr->addMember( "z", new BLong( slist[i].z ) );
                            arr->addMember( "objtype", new BLong( slist[i].objtype ) );
                            newarr->addElement( arr );
                        }
                    }
                }

                if (!( flags & ITEMS_IGNORE_MULTIS ))
                {
                    StaticList mlist;
                    realm->readmultis( mlist, wx, wy );

                    for( unsigned i = 0; i < mlist.size(); ++i )
                    {
                        if ((z == LIST_IGNORE_Z) || (abs(mlist[i].z - z) < CONST_DEFAULT_ZRANGE))
                        {
                            BStruct* arr = new BStruct;
                            arr->addMember( "x", new BLong( wx ) );
                            arr->addMember( "y", new BLong( wy ) );
                            arr->addMember( "z", new BLong( mlist[i].z ) );
                            arr->addMember( "objtype", new BLong( mlist[i].graphic ) );
                            newarr->addElement( arr );
                        }
                    }
                }
            }
        }
        
        return newarr.release();
    }
    else
        return new BError( "Invalid parameter" );
}

//  Birdy :  (Notes on Pathfinding)
//
//  This implimentation of pathfinding is actually from a very nice stl based A*
//  implimentation.  It will, within reason, succeed in finding a path in a wide
//  variety of circumstance, including traversing stairs and multiple level
//  structures.
//
//  Unfortunately, it, or my POL support classes implimenting it, seems to at times
//  generate corrupted results in finding a path.  These corruptions take the form
//  of basically looping within the result list, or linking in non-closed list nodes
//  which may be on the open list and generate more looping or which may have even
//  been deleted by the algorithm, generally causing an illegal reference and crash.
//
//  Thus far, the above senerio has only been replicated, though fairly reliably, in
//  cases where there are many many(about 100) npc's trying to pathfind at the same time,
//  in close proximity to one another.
//
//  "Looping" will, of course, cause the shard to hang while the pathfinding routine
//  tries to build the child list for the solution set.  An illegal reference will
//  cause the shard to crash most likely.  Neither is tollerable of course, so I have
//  put in two safeguards against this.
//
//  Safeguard #1) As we go through the nodes in the solution, I make sure each of them
//                is on the Closed List.  If not, the pathfind errors out with a
//                "Solution Corrupt!" error.  This is an attempt to short circuit any
//                solution containing erroneous nodes in it.
//
//  Safeguard #2) I keep a vector of the nodes in the solution as we go through them.
//                Before adding each node to the vector, I search that vector for that
//                node.  If that vector already contains the node, the pathfind errors
//                out with a "Solution Corrupt!" error.  THis is an attempt to catch
//                the cases where Closed List nodes have looped for some reason.
//
//  These two safeguards should not truly be necessary for this algorithm, and take up
//  space and time to guard against.  Thus, finding out why these problems are occurring
//  in the first place would be great, as we could fix that instead and remove these
//  checks.  If necessary to live with long term, then the solution vector should probably
//  be made into a hash table for quick lookups.
//
//  I have also implimented a sort blocking list that is supposed to represent those
//  spots that mobiles occupy and are thus not walkable on.  This is only maintained if
//  the mobilesBlock parameter passed from escript is true.  It would probably be good
//  to make this a hash table.  It may be said that it is a good idea to pre-process all
//  blocking spots on the map, but this would hinder the ability to manage 3d shifts, or
//  probably be prohibitive if an attempt were made to check for traversal at all possible
//  various z's.  So it may be that checking such at the time of the search for the path
//  is ultimately the best means of handling this.
//
//  Other details that may be nice to have here might be a method for interacting with
//  escript, letting escript tell the pathfinder to ignore doors, and then the pathfinder
//  putting in the solution list an indication to escript that at the given node a door
//  was encountered, so that AI can open the door before attempting to traverse that
//  path.  Providing the user the ability to define an array of objects which are to be
//  considered as blocking, or an array of objects which are to be ignored even if they
//  are blocking might be nice as well.  The former could be easily implimented by
//  adding to the mobile's blocking list.  The latter may be more difficult to do
//  considering if the item blocks, the walk function will not let you traverse it, and
//  it is the walk function that we use to accurately replicate the ability of the
//  mobile to traverse from one place to the next.  Additional functionality of some
//  sort may be necessary to do this, or making a copy of the walk function and putting
//  in a special walk function strictly for pathfinding which can take this array into
//  account.
//
//  For now, however, these concerns are all unaddressed.  I wish to see if this
//  function works and is useful, and also would prefer to find some way to track down
//  the corruption of the solution nodes before adding more complexity to the problem.
//
//  Other than the below function, the following files will be added to CVS to support
//  it in the src module.  They are :
//
//  stlastar.h  --  virtually untouched by me really, it is the original author's
//                  implimentation, pretty nicely done and documented if you are
//                  interested in learning about A*.
//
//  fsa.h       --  a "fixed size allocation" module which I toy with enabling and
//                  disabling.  Using it is supposed to get you some speed, but it
//                  limits your maps because it will only allocate a certain # of
//                  nodes at once, and after that, it will return out of memory.
//                  Presently, it is disabled, but will be submitted to CVS for
//                  completion in case we wish to use it later.
//
//  uopathnode.h -- this is stricly my work, love it or hate it, it's pretty quick
//                  and dirty, and can stand to be cleaned up, optimized, and so forth.
//                  I have the name field and function in there to allow for some
//                  debugging, though the character array could probably be removed to
//                  make the nodes smaller.  I didn't find it mattered particularly, since
//                  these puppies are created and destroyed at a pretty good clip.
//                  It is this class that encapsulates the necessary functionality to
//                  make the otherwise fairly generic stlastar class work.

typedef AStarSearch<UOPathState> UOSearch;

BObjectImp* UOExecutorModule::mf_FindPath()
{
    long x1, x2;
    long y1, y2;
    long z1, z2;
	long flags;
	long theSkirt;
	const String* strrealm;
	Realm* realm;
	if (getParam( 0, x1 ) &&
        getParam( 1, y1 ) &&
        getParam( 2, z1, WORLD_MIN_Z, WORLD_MAX_Z ) &&
		getParam( 3, x2) &&
		getParam( 4, y2) &&
        getParam( 5, z2, WORLD_MIN_Z, WORLD_MAX_Z ) )
    {
		if (pol_distance((unsigned short) x1,(unsigned short) y1,(unsigned short) x2,(unsigned short) y2) > ssopt.max_pathfind_range)
			return new BError("Beyond Max Range.");

		if(!getStringParam(6,strrealm))
			strrealm = new String("britannia");

		if (!getParam(7, flags))
			flags = FP_IGNORE_MOBILES;

		if (!getParam(8, theSkirt))
			theSkirt = 5;

		if (theSkirt < 0)
			theSkirt = 0;

		realm = find_realm(strrealm->value());
		if(!realm) return new BError("Realm not found");
		if(!realm->valid(x1,y1,z1)) return new BError("Start Coordinates Invalid for Realm");
		if(!realm->valid(x2,y2,z2)) return new BError("End Coordinates Invalid for Realm");
		UOSearch * astarsearch;
		unsigned int SearchState;
        astarsearch = new UOSearch;
		unsigned int xL, xH, yL, yH;

		if (x1 < x2)
		{
			xL = x1 - theSkirt;
			xH = x2 + theSkirt;
		}
		else
		{
			xH = x1 + theSkirt;
			xL = x2 - theSkirt;
		}

		if (y1 < y2)
		{
			yL = y1 - theSkirt;
			yH = y2 + theSkirt;
		}
		else
		{
			yH = y1 + theSkirt;
			yL = y2 - theSkirt;
		}

		if (xL < 0)
			xL = 0;
		if (yL < 0)
			yL = 0;
		if (xH >= realm->width())
			xH = realm->width() - 1;
		if (yH >= realm->height())
			yH = realm->height() - 1;

        if (config.loglevel >= 12)
		{
            Log( "[FindPath] Calling FindPath(%d, %d, %d, %d, %d, %d, %s, 0x%x, %d)\n",
			  x1, y1, z1, x2, y2, z2, strrealm->data(), flags, theSkirt);
            Log( "[FindPath]   search for Blockers inside %d %d %d %d\n", xL, yL, xH, yH);
		}

		AStarBlockers theBlockers(xL, xH, yL, yH);

	    unsigned short wxL, wyL, wxH, wyH;
		if (!(flags & FP_IGNORE_MOBILES))
		{
			zone_convert_clip( xL, yL, realm, wxL, wyL );
			zone_convert_clip( xH, yH, realm, wxH, wyH );
			for( unsigned short wx = wxL; wx <= wxH; ++wx )
			{
				for( unsigned short wy = wyL; wy <= wyH; ++wy )
				{
					ZoneCharacters& wchr = realm->zone[wx][wy].characters;
					for( ZoneCharacters::iterator itr = wchr.begin(), end = wchr.end(); itr != end; ++itr )
			        {
				        Character* chr = *itr;
						theBlockers.AddBlocker(chr->x, chr->y, chr->z);

						if (config.loglevel >= 12)
							Log( "[FindPath]     add Blocker %s at %d %d %d\n",
								chr->name().c_str(), chr->x, chr->y, chr->z);
					}
				}
			}
		}

		// passed via GetSuccessors to realm->walkheight
		bool doors_block = (flags & FP_IGNORE_DOORS) ? false : true;

		if (config.loglevel >= 12)
		{
            Log( "[FindPath]   use StartNode %d %d %d\n", x1, y1, z1);
            Log( "[FindPath]   use EndNode %d %d %d\n", x2, y2, z2);
		}

		// Create a start state
		UOPathState nodeStart( x1, y1, z1, realm, &theBlockers );
		// Define the goal state
		UOPathState nodeEnd( x2, y2, z2, realm, &theBlockers );
		// Set Start and goal states
		astarsearch->SetStartAndGoalStates( nodeStart, nodeEnd );
		do
		{
			SearchState = astarsearch->SearchStep(doors_block);
		}
		while( SearchState == UOSearch::SEARCH_STATE_SEARCHING );
		if( SearchState == UOSearch::SEARCH_STATE_SUCCEEDED )
		{
			UOPathState *node = astarsearch->GetSolutionStart();
			ObjArray* nodeArray = NULL;
			BStruct* nextStep = NULL;

			nodeArray = new ObjArray();
			while ((node = astarsearch->GetSolutionNext()) != NULL)
			{
				nextStep = new BStruct;
				nextStep->addMember("x", new BLong(node->x));
				nextStep->addMember("y", new BLong(node->y));
				nextStep->addMember("z", new BLong(node->z));
				nodeArray->addElement(nextStep);
			}
			astarsearch->FreeSolutionNodes();
			delete astarsearch;
			return nodeArray;
		}
		else if (SearchState == UOSearch::SEARCH_STATE_FAILED)
		{
			delete astarsearch;
			return new BError("Failed to find a path.");
		}
		else if (SearchState == UOSearch::SEARCH_STATE_OUT_OF_MEMORY)
		{
			delete astarsearch;
			return new BError("Out of memory.");
		}
		else if (SearchState == UOSearch::SEARCH_STATE_SOLUTION_CORRUPTED)
		{
			delete astarsearch;
			return new BError("Solution Corrupted!");
		}

		delete astarsearch;
		return new BError("Pathfind Error.");
    }
    else
    {
		return new BError("Invalid parameter");
    }
}


BObjectImp* UOExecutorModule::mf_UseItem()
{
    Item* item;
    Character* chr;

	if (getItemParam( exec, 0, item ) && getCharacterParam(exec, 1, chr))
	{

		const ItemDesc& itemdesc = find_itemdesc( item->objtype_ );

		if (itemdesc.requires_attention && (chr->skill_ex_active() ||
											chr->casting_spell()))
		{
			if (chr->client != NULL)
			{
				send_sysmessage( chr->client, "I am already doing something else." );
				return new BError("Character busy.");;
			}
		}

		if (itemdesc.requires_attention && chr->hidden())
			chr->unhide();

		ref_ptr<EScriptProgram> prog;

		std::string on_use_script = item->get_use_script_name();

		if (!on_use_script.empty())
		{
			ScriptDef sd( on_use_script, NULL, "" );
			prog = find_script2( sd,
								true, // complain if not found
								config.cache_interactive_scripts );
		}
		else if (!itemdesc.on_use_script.empty())
		{
			prog = find_script2( itemdesc.on_use_script,
						       true,
						       config.cache_interactive_scripts );
		}

		if (prog.get() != NULL)
		{
			if (chr->start_itemuse_script( prog.get(), item, itemdesc.requires_attention ))
				return new BLong(1);
			else
				return new BError("Failed to start script!");
			// else log the fact?
		}
		else
		{
			if (chr->client != NULL)
				item->builtin_on_use( chr->client );
			return new BLong(0);
		}
	}
	else
	{
		return new BError("Invalid parameter");
	}
}

BObjectImp* UOExecutorModule::mf_FindSubstance()
{
	UContainer::Contents substanceVector;

    Item* cont_item;
    unsigned short objtype;
    long amount;
	long amthave;
	long makeInUseLong;
	bool makeInUse;

	if (getItemParam( exec, 0, cont_item ) &&
        getObjtypeParam( exec, 1, objtype ) &&
        getParam( 2, amount ))
    {
		if (!getParam(3, makeInUseLong))
			makeInUse = false;
		else
			makeInUse = (makeInUseLong ? true : false);

		if (!cont_item->isa( UObject::CLASS_CONTAINER ))
            return new BError( "That is not a container" );
        if (amount < 0)
            return new BError( "Amount cannot be negative" );

        UContainer* cont = static_cast<UContainer*>(cont_item);
        amthave = cont->find_sumof_objtype_noninuse(objtype, amount, substanceVector);
        if (amthave < amount)
            return new BError( "Not enough of that substance in container" );
		else
		{
			ObjArray* theArray = new ObjArray();
			Item * item;

			for( UContainer::Contents::const_iterator itr = substanceVector.begin(); itr != substanceVector.end(); ++itr )
			{
				item = GET_ITEM_PTR( itr );
				if (item != NULL)
				{
					if ((makeInUse) && (!item->inuse()))
					{
						item->inuse( true );
						reserved_items_.push_back( ItemRef(item) );
					}
					theArray->addElement( new EItemRefObjImp( item ) );
				}
			}
			return theArray;
		}
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

UOFunctionDef UOExecutorModule::function_table[] =
{
	{ "SendStatus",					&UOExecutorModule::mf_SendStatus },

    { "SendCharacterRaceChanger",	&UOExecutorModule::mf_SendCharacterRaceChanger },
    { "SendHousingTool",		&UOExecutorModule::mf_SendHousingTool },
	{ "MoveObjectToLocation",	&UOExecutorModule::mf_MoveObjectToLocation },
	{ "SendOpenBook",           &UOExecutorModule::mf_SendOpenBook },
    { "SelectColor",            &UOExecutorModule::mf_SelectColor },
    { "AddAmount",              &UOExecutorModule::mf_AddAmount },
    { "SendViewContainer",      &UOExecutorModule::mf_SendViewContainer },
    { "SendInstaResDialog",     &UOExecutorModule::mf_SendInstaResDialog },
//    { "AwardRawPoints",         &UOExecutorModule::mf_AwardRawPoints },
    { "SendStringAsTipWindow",  &UOExecutorModule::mf_SendStringAsTipWindow },
    { "GetCommandHelp",         &UOExecutorModule::mf_GetCommandHelp },
    { "PlaySoundEffectPrivate", &UOExecutorModule::mf_PlaySoundEffectPrivate },
    { "ConsumeSubstance",       &UOExecutorModule::mf_ConsumeSubstance },
	{ "FindSubstance",          &UOExecutorModule::mf_FindSubstance },
	{ "Shutdown",               &UOExecutorModule::mf_Shutdown },
    { "OpenPaperdoll",          &UOExecutorModule::mf_OpenPaperdoll },
    { "SendSkillWindow",        &UOExecutorModule::mf_SendSkillWindow },
    { "ReserveItem",            &UOExecutorModule::mf_ReserveItem },
    { "ReleaseItem",            &UOExecutorModule::mf_ReleaseItem },
    { "GetStandingHeight",      &UOExecutorModule::mf_GetStandingHeight },
    { "GetStandingLayers",      &UOExecutorModule::mf_GetStandingLayers },
    { "AssignRectToWeatherRegion",  &UOExecutorModule::mf_AssignRectToWeatherRegion },
    { "CreateAccount",          &UOExecutorModule::mf_CreateAccount },
    { "FindAccount",            &UOExecutorModule::mf_FindAccount },
    { "ListAccounts",           &UOExecutorModule::mf_ListAccounts },

    // { "AssignMultiComponent",   &UOExecutorModule::mf_AssignMultiComponent },
    { "SetScriptController",    &UOExecutorModule::mf_SetScriptController },
    { "PolCore",                &UOExecutorModule::mf_PolCore },
    { "GetWorldHeight",         &UOExecutorModule::mf_GetWorldHeight },
//    { "GameStat",               &UOExecutorModule::mf_GameStat },
    { "StartSpellEffect",       &UOExecutorModule::mf_StartSpellEffect },
    { "GetSpellDifficulty",     &UOExecutorModule::mf_GetSpellDifficulty },
    { "SpeakPowerWords",        &UOExecutorModule::mf_SpeakPowerWords },
	{ "GetMultiDimensions",     &UOExecutorModule::mf_GetMultiDimensions },
    { "DestroyMulti",           &UOExecutorModule::mf_DestroyMulti },
    { "SendTextEntryGump",      &UOExecutorModule::mf_SendTextEntryGump },
    { "SendDialogGump",         &UOExecutorModule::mf_SendGumpMenu },
    { "SendEvent",              &UOExecutorModule::mf_SendEvent },
    { "PlayMovingEffectXyz",    &UOExecutorModule::mf_PlayMovingEffectXyz },
    { "GetEquipmentByLayer",    &UOExecutorModule::mf_GetEquipmentByLayer },
    { "GetObjtypeByName",       &UOExecutorModule::mf_GetObjtypeByName },
    { "ListHostiles",           &UOExecutorModule::mf_ListHostiles },
    { "DisconnectClient",       &UOExecutorModule::mf_DisconnectClient },
	{ "GetRegionName",			&UOExecutorModule::mf_GetRegionName },
	{ "GetRegionNameAtLocation",&UOExecutorModule::mf_GetRegionNameAtLocation },
    { "GetRegionString",        &UOExecutorModule::mf_GetRegionString },
    { "PlayStationaryEffect",   &UOExecutorModule::mf_PlayStationaryEffect },
    { "GetMapInfo",             &UOExecutorModule::mf_GetMapInfo },
    { "ListObjectsInBox",       &UOExecutorModule::mf_ListObjectsInBox },
    { "ListMultisInBox",        &UOExecutorModule::mf_ListMultisInBox },
    { "ListStaticsInBox",       &UOExecutorModule::mf_ListStaticsInBox },
    { "ListEquippedItems",      &UOExecutorModule::mf_ListEquippedItems },
    { "ConsumeReagents",        &UOExecutorModule::mf_ConsumeReagents },
    { "SendPacket",             &UOExecutorModule::mf_SendPacket },
	{ "SendQuestArrow",         &UOExecutorModule::mf_SendQuestArrow },
    { "RequestInput",           &UOExecutorModule::mf_PromptInput },
    { "ReadGameClock",          &UOExecutorModule::mf_ReadGameClock },
    { "GrantPrivilege",         &UOExecutorModule::mf_GrantPrivilege },
    { "RevokePrivilege",        &UOExecutorModule::mf_RevokePrivilege },
    { "EquipFromTemplate",      &UOExecutorModule::mf_EquipFromTemplate },
    { "GetHarvestDifficulty",   &UOExecutorModule::mf_GetHarvestDifficulty },
    { "HarvestResource",        &UOExecutorModule::mf_HarvestResource },
    { "RestartScript",          &UOExecutorModule::mf_RestartScript },
    { "EnableEvents",           &UOExecutorModule::mf_EnableEvents },
    { "DisableEvents",          &UOExecutorModule::mf_DisableEvents },
    { "EquipItem",              &UOExecutorModule::mf_EquipItem },
    { "MoveItemToContainer",    &UOExecutorModule::mf_MoveItemToContainer },
    { "MoveItemToSecureTradeWin", &UOExecutorModule::mf_MoveItemToSecureTradeWin },
    { "FindObjtypeInContainer", &UOExecutorModule::mf_FindObjtypeInContainer },
    { "SendOpenSpecialContainer", &UOExecutorModule::mf_SendOpenSpecialContainer },
    { "SecureTradeWin",         &UOExecutorModule::mf_SecureTradeWin },
	{ "CloseTradeWindow",		&UOExecutorModule::mf_CloseTradeWindow },
    { "SendBuyWindow",          &UOExecutorModule::mf_SendBuyWindow },
    { "SendSellWindow",         &UOExecutorModule::mf_SendSellWindow },
    { "CreateItemInContainer",  &UOExecutorModule::mf_CreateItemInContainer },
    { "CreateItemInInventory",  &UOExecutorModule::mf_CreateItemInInventory },
    { "ListMobilesNearLocationEx",      &UOExecutorModule::mf_ListMobilesNearLocationEx },
    { "SystemFindObjectBySerial",       &UOExecutorModule::mf_SystemFindObjectBySerial },
    { "ListItemsNearLocationOfType",    &UOExecutorModule::mf_ListItemsNearLocationOfType },
	{ "ListItemsNearLocationWithFlag", &UOExecutorModule::mf_ListItemsNearLocationWithFlag },
	{ "ListStaticsAtLocation",     &UOExecutorModule::mf_ListStaticsAtLocation },
	{ "ListStaticsNearLocation",   &UOExecutorModule::mf_ListStaticsNearLocation },
    { "ListGhostsNearLocation", &UOExecutorModule::mf_ListGhostsNearLocation },
    { "ListMobilesInLineOfSight", &UOExecutorModule::mf_ListMobilesInLineOfSight },
    { "Distance",               &UOExecutorModule::mf_Distance },
	{ "CoordinateDistance",		&UOExecutorModule::mf_CoordinateDistance },
	{ "GetCoordsInLine",		&UOExecutorModule::mf_GetCoordsInLine },
	{ "GetFacing",				&UOExecutorModule::mf_GetFacing },
    { "SetRegionLightLevel",    &UOExecutorModule::mf_SetRegionLightLevel },
    { "SetRegionWeatherLevel",  &UOExecutorModule::mf_SetRegionWeatherLevel },
    { "EraseObjProperty",       &UOExecutorModule::mf_EraseObjProperty },
    { "GetGlobalProperty",      &UOExecutorModule::mf_GetGlobalProperty },
    { "SetGlobalProperty",      &UOExecutorModule::mf_SetGlobalProperty },
    { "EraseGlobalProperty",    &UOExecutorModule::mf_EraseGlobalProperty },
    { "SaveWorldState",         &UOExecutorModule::mf_SaveWorldState },
    { "CreateMultiAtLocation",  &UOExecutorModule::mf_CreateMultiAtLocation },
    { "TargetMultiPlacement",   &UOExecutorModule::mf_TargetMultiPlacement },
    { "Resurrect",              &UOExecutorModule::mf_Resurrect },
    { "CreateNpcFromTemplate",  &UOExecutorModule::mf_CreateNpcFromTemplate },
//    { "SetRawSkill",            &UOExecutorModule::mf_SetRawSkill },
//    { "GetRawSkill",            &UOExecutorModule::mf_GetRawSkill },
    { "RegisterForSpeechEvents", &UOExecutorModule::mf_RegisterForSpeechEvents },
    { "EnumerateOnlineCharacters", &UOExecutorModule::mf_EnumerateOnlineCharacters },
    { "PrintTextAbove",         &UOExecutorModule::mf_PrintTextAbove },
    { "PrintTextAbovePrivate",  &UOExecutorModule::mf_PrivateTextAbove },

    { "Accessible",             &UOExecutorModule::mf_Accessible },
    { "ApplyConstraint",        &UOExecutorModule::mf_ApplyConstraint },
    { "Attach",                 &UOExecutorModule::mf_Attach },
	{ "broadcast",              &UOExecutorModule::broadcast },
    { "CheckLineOfSight",       &UOExecutorModule::mf_CheckLineOfSight },
    { "CheckLosAt",             &UOExecutorModule::mf_CheckLosAt },
    
    { "CreateItemInBackpack",   &UOExecutorModule::mf_CreateItemInBackpack },
    { "CreateItemAtLocation",   &UOExecutorModule::mf_CreateItemAtLocation },

	{ "CreateItemCopyAtLocation",   &UOExecutorModule::mf_CreateItemCopyAtLocation },

//    { "Damage",                 &UOExecutorModule::mf_ApplyRawDamage },
    
    { "DestroyItem",            &UOExecutorModule::mf_DestroyItem },
    { "Detach",                 &UOExecutorModule::mf_Detach },
    { "EnumerateItemsInContainer", &UOExecutorModule::mf_EnumerateItemsInContainer },
	{ "FindPath",               &UOExecutorModule::mf_FindPath },
	{ "GetAmount",              &UOExecutorModule::mf_GetAmount },
    { "GetMenuObjTypes",        &UOExecutorModule::mf_GetMenuObjTypes },
    { "GetObjProperty",         &UOExecutorModule::mf_GetObjProperty },
    { "GetObjPropertyNames",    &UOExecutorModule::mf_GetObjPropertyNames },
    { "GetObjType",             &UOExecutorModule::mf_GetObjType },
    { "GetPosition",            &UOExecutorModule::mf_GetPosition },
    { "ListItemsAtLocation",    &UOExecutorModule::mf_ListItemsAtLocation },
    { "ListItemsNearLocation",  &UOExecutorModule::mf_ListItemsNearLocation },
    { "ListMobilesNearLocation", &UOExecutorModule::mf_ListMobilesNearLocation },
    { "PerformAction",          &UOExecutorModule::mf_PerformAction },
    { "PlayLightningBoltEffect", &UOExecutorModule::mf_PlayLightningBoltEffect },
    { "PlayMovingEffect",       &UOExecutorModule::mf_PlayMovingEffect },
    { "PlayObjectCenteredEffect", &UOExecutorModule::mf_PlayObjectCenteredEffect },
    { "PlaySoundEffect",        &UOExecutorModule::mf_PlaySoundEffect },
    { "SelectMenuItem2",        &UOExecutorModule::mf_SelectMenuItem },
    { "SendSysMessage",         &UOExecutorModule::mf_SendSysMessage },
	{ "SetObjProperty",         &UOExecutorModule::mf_SetObjProperty },
    { "SetName",                &UOExecutorModule::mf_SetName },
    { "SubtractAmount",         &UOExecutorModule::mf_SubtractAmount },
    { "Target",                 &UOExecutorModule::mf_Target },
    { "TargetCoordinates",      &UOExecutorModule::mf_TargetCoordinates },
    { "CancelTarget",           &UOExecutorModule::mf_TargetCancel },
	{ "UseItem",                &UOExecutorModule::mf_UseItem },

    { "CreateMenu",             &UOExecutorModule::mf_CreateMenu },
    { "AddMenuItem",            &UOExecutorModule::mf_AddMenuItem }
};

typedef map< string, int, ci_cmp_pred > FuncIdxMap;
FuncIdxMap funcmap;
bool funcmap_init = false;

int UOExecutorModule::functionIndex( const char *name )
{
    if (!funcmap_init)
    {
	    for( unsigned idx = 0; idx < arsize(function_table); idx++ )
	    {
            funcmap[ function_table[idx].funcname ] = idx;
	    }
        funcmap_init = true;
    }

    FuncIdxMap::iterator itr = funcmap.find( name );
    if (itr != funcmap.end())
        return (*itr).second;
    else
        return -1;
}

BObjectImp* UOExecutorModule::execFunc( unsigned funcidx )
{
	return callMemberFunction(*this, function_table[funcidx].fptr)();
};
