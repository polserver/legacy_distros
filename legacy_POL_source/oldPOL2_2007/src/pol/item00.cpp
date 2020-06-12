#include "clib/stl_inc.h"
#include "clib/endian.h"

#include "gameclck.h"
#include "item.h"
#include "itemdesc.h"
#include "layers.h"
#include "resource.h"
#include "objtype.h"
#include "uofile.h"
#include "ustruct.h"
#include "uobjcnt.h"

Item::Item( const ItemDesc& id, UOBJ_CLASS uobj_class) :
	UObject( id.objtype, uobj_class ),
	container(NULL),
    decayat_gameclock_(0),
    sellprice_(ULONG_MAX), //dave changed 1/15/3 so 0 means 0, not default to itemdesc value
    buyprice_(ULONG_MAX),  //dave changed 1/15/3 so 0 means 0, not default to itemdesc value
	amount_(1),
    newbie_(id.newbie),
    movable_(id.default_movable()),
    inuse_(false),
	is_gotten_(0),
    invisible_(id.invisible),
	layer(0)
{
    graphic = id.graphic;
    graphic_ext = ctBEu16( graphic );
    color = id.color;
    color_ext = ctBEu16( color );
    setfacing( id.facing );
    equip_script_ = id.equip_script;
    unequip_script_ = id.unequip_script;
    if (objtype_ == EXTOBJ_MOUNT)
        layer = LAYER_MOUNT;

    ++uitem_count;

    // hmm, doesn't quite work right with items created on startup..
    decayat_gameclock_ = read_gameclock() + id.decay_time * 60;
    //existing_items.insert( this );
}

Item::~Item()
{
    --uitem_count;
    return_resources( objtype_, amount_ );
    //existing_items.erase( this );
}
