// History
//   2005/01/24 Shinigami: added ObjMember character.spyonclient2 to get data from packet 0xd9 (Spy on Client 2)
//   2005/03/09 Shinigami: Added Prop Character::Delay_Mod [ms] for WeaponDelay
//   2005/04/02 Shinigami: UBoat::script_method_id & UBoat::script_method - added optional realm param
//   2005/04/04 Shinigami: Added Prop Character::CreatedAt [PolClock]
//   2005/08/29 Shinigami: get-/setspyonclient2 renamed to get-/setclientinfo
//   2005/11/26 Shinigami: changed "strcmp" into "stricmp" to suppress Script Errors
//   2005/12/09 MuadDib: Added uclang member for storing UC language from client.
//   2005/12/09 MuadDib: Fixed ~ItemGivenEvent not returning items correctly if the script
//					   did nothing with it. Was incorrect str/int comparision for times.
//   2006/05/16 Shinigami: added Prop Character.Race [RACE_* Constants] to support Elfs
//   2006/06/15 Austin: Added mobile.Privs()
//   2007/07/09 Shinigami: added Prop Character.isUOKR [bool] - UO:KR client used?

#include "clib/stl_inc.h"
#ifdef _MSC_VER
#pragma warning( disable: 4786 )
#endif


#include "bscript/berror.h"
#include "bscript/dict.h"
#include "bscript/escrutil.h"
#include "bscript/execmodl.h"
#include "bscript/impstr.h"
#include "bscript/objmembers.h"
#include "bscript/objmethods.h"

#include "clib/endian.h"
#include "clib/mlog.h"
#include "clib/stlutil.h"
#include "clib/strutil.h"
#include "clib/unicode.h"

#include "plib/realm.h"

#include "account.h"
#include "acscrobj.h"
#include "armor.h"
#include "armrtmpl.h"
#include "attribute.h"
#include "boatcomp.h"
#include "charactr.h"
#include "client.h"
#include "cmdlevel.h"
#include "door.h"
#include "exscrobj.h"
#include "fnsearch.h"
#include "guilds.h"
#include "house.h"
#include "item.h"
#include "umap.h"
#include "npc.h"
#include "objtype.h"
#include "polclass.h"
#include "realms.h"
#include "spelbook.h"
#include "statmsg.h"
#include "syshookscript.h"
#include "ufunc.h"
#include "uofile.h"
#include "weapon.h"
#include "wepntmpl.h"
#include "uoemod.h"
#include "uoexhelp.h"
#include "uworld.h"

#include "uoscrobj.h"


///$TITLE=Game Element Scripting Objects

BApplicObjType euboatrefobjimp_type;
BApplicObjType emultirefobjimp_type;
BApplicObjType eitemrefobjimp_type;
BApplicObjType echaracterrefobjimp_type;
BApplicObjType echaracterequipobjimp_type;
BApplicObjType storage_area_type;
BApplicObjType bounding_box_type;
BApplicObjType menu_type;

const char* ECharacterRefObjImp::typeOf() const
{
	return "MobileRef";
}

BObjectImp* ECharacterRefObjImp::copy() const
{
	return new ECharacterRefObjImp( obj_.get() );
}

//////id test
BObjectRef ECharacterRefObjImp::get_member_id( const int id )
{
	BObjectImp* result = obj_->get_script_member_id( id );
	if (result != NULL)
		return BObjectRef(result);
	else
		return BObjectRef(UninitObject::create());
}

//////

BObjectRef ECharacterRefObjImp::get_member( const char* membername )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if( objmember != NULL )
		return this->get_member_id(objmember->id);
	else
		return BObjectRef(UninitObject::create());
}

BObjectRef ECharacterRefObjImp::set_member_id( const int id, BObjectImp* value ) //id test
{
	BObjectImp* result = NULL;
	if (value->isa( BObjectImp::OTLong ))
	{
		BLong* lng = static_cast<BLong*>(value);
		result = obj_->set_script_member_id( id, lng->value() );
	}
	else if (value->isa( BObjectImp::OTString ))
	{
		String* str = static_cast<String*>(value);
		result = obj_->set_script_member_id( id, str->value() );
	}
	if (result != NULL)
	{
		return BObjectRef( result );
	}
	else
	{
		return BObjectRef(UninitObject::create());
	}
}

BObjectRef ECharacterRefObjImp::set_member( const char* membername, BObjectImp* value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_member_id(objmember->id, value);
	else
		return BObjectRef(UninitObject::create());
}

BObjectImp* ECharacterRefObjImp::call_method_id( const int id, Executor& ex )
{
	if (!obj_->orphan())
	{
		BObjectImp* imp = obj_->script_method_id( id, ex );
		if (imp != NULL)
			return imp;
		else
			return base::call_method_id( id, ex );
	}
	else
	{
		return new BError( "That object no longer exists" );
	}
}

BObjectImp* ECharacterRefObjImp::call_method( const char* methodname, Executor& ex )
{
	ObjMethod* objmethod = getKnownObjMethod(methodname);
	if ( objmethod != NULL )
		return this->call_method_id(objmethod->id, ex);
	else
		return base::call_method(methodname, ex);
}

bool ECharacterRefObjImp::isTrue() const
{
	return (!obj_->orphan() && obj_->logged_in);
}

bool ECharacterRefObjImp::isEqual(const BObjectImp& objimp) const
{
	if (objimp.isa( BObjectImp::OTApplicObj ))
	{
		const BApplicObjBase* aob = explicit_cast<const BApplicObjBase*, const BObjectImp*>(&objimp);

		if (aob->object_type() == &echaracterrefobjimp_type)
		{
			const ECharacterRefObjImp* chrref_imp =
					explicit_cast<const ECharacterRefObjImp*,const BApplicObjBase*>(aob);

			return (chrref_imp->obj_->serial == obj_->serial);
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

bool ECharacterRefObjImp::isLessThan(const BObjectImp& objimp) const
{
	if (objimp.isa( BObjectImp::OTApplicObj ))
	{
		const BApplicObjBase* aob = explicit_cast<const BApplicObjBase*, const BObjectImp*>(&objimp);

		if (aob->object_type() == &echaracterrefobjimp_type)
		{
			const ECharacterRefObjImp* chrref_imp =
					explicit_cast<const ECharacterRefObjImp*,const BApplicObjBase*>(aob);

			return (chrref_imp->obj_->serial < obj_->serial);
		}
		else if (aob->object_type() == &eitemrefobjimp_type)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

//////////////
const char* EOfflineCharacterRefObjImp::typeOf() const
{
	return "OfflineMobileRef";
}

BObjectImp* EOfflineCharacterRefObjImp::copy() const
{
	return new EOfflineCharacterRefObjImp( obj_.get() );
}

bool EOfflineCharacterRefObjImp::isTrue() const
{
	return (!obj_->orphan());
}
///////////////


const char* EItemRefObjImp::typeOf() const
{
	return "ItemRef";
}

BObjectImp* EItemRefObjImp::copy() const
{
	return new EItemRefObjImp( obj_.get() );
}

BObjectRef EItemRefObjImp::get_member_id( const int id ) //id test
{
	BObjectImp* result = obj_->get_script_member_id( id );
	if (result != NULL)
		return BObjectRef(result);
	else
		return BObjectRef(UninitObject::create());
}

BObjectRef EItemRefObjImp::get_member( const char* membername )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->get_member_id(objmember->id);
	else
		return BObjectRef(UninitObject::create());
}

BObjectRef EItemRefObjImp::set_member_id( const int id, BObjectImp* value ) //id test
{
	BObjectImp* result = NULL;
	if (value->isa( BObjectImp::OTLong ))
	{
		BLong* lng = static_cast<BLong*>(value);
		result = obj_->set_script_member_id( id, lng->value() );
	}
	else if (value->isa( BObjectImp::OTString ))
	{
		String* str = static_cast<String*>(value);
		result = obj_->set_script_member_id( id, str->value() );
	}
	else if (value->isa( BObjectImp::OTDouble ))
	{
		Double* dbl = static_cast<Double*>(value);
		result = obj_->set_script_member_id_double( id, dbl->value() );
	}
	if (result != NULL)
	{
		return BObjectRef( result );
	}
	else
	{
		return BObjectRef(UninitObject::create());
	}
}

BObjectRef EItemRefObjImp::set_member( const char* membername, BObjectImp* value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_member_id(objmember->id, value);
	else
		return BObjectRef(UninitObject::create());
}

BObjectImp* EItemRefObjImp::call_method_id( const int id, Executor& ex )
{
	Item* item = obj_.get();
	if (!item->orphan())
	{
		ObjMethod* mth = getObjMethod(id);
		if ( mth->overridden )
		{
			BObjectImp* imp = item->custom_script_method( mth->code, ex );
			if (imp)
				return imp;
		}

		BObjectImp* imp = item->script_method_id( id, ex );
		if (imp != NULL)
			return imp;
		else
			return base::call_method_id( id, ex );
	}
	else
	{
		return new BError( "That object no longer exists" );
	}
}

BObjectImp* EItemRefObjImp::call_method( const char* methodname, Executor& ex )
{
	if (methodname[0] == '_')
		++methodname;
	
	ObjMethod* objmethod = getKnownObjMethod(methodname);
	if ( objmethod != NULL )
		return this->call_method_id(objmethod->id, ex);
	else
	{
		Item* item = obj_.get();
		BObjectImp* imp = item->custom_script_method( methodname, ex );
		if ( imp )
			return imp;
		else
			return base::call_method( methodname, ex );
	}
	/*
	Item* item = obj_.get();
	if (!item->orphan())
	{
		if (methodname[0] == '_')
		{
			++methodname;
		}
		else
		{
			BObjectImp* imp = item->custom_script_method( methodname, ex );
			if (imp)
				return imp;
		}

		BObjectImp* imp = item->script_method( methodname, ex );
		if (imp != NULL)
			return imp;
		else
			return base::call_method( methodname, ex );
	}
	else
	{
		return new BError( "That object no longer exists" );
	}
	*/
}

bool EItemRefObjImp::isTrue() const
{
	return (!obj_->orphan());
}

bool EItemRefObjImp::isEqual(const BObjectImp& objimp) const
{
	if (objimp.isa( BObjectImp::OTApplicObj ))
	{
		const BApplicObjBase* aob = explicit_cast<const BApplicObjBase*, const BObjectImp*>(&objimp);

		if (aob->object_type() == &eitemrefobjimp_type)
		{
			const EItemRefObjImp* itemref_imp =
					explicit_cast<const EItemRefObjImp*,const BApplicObjBase*>(aob);

			return (itemref_imp->obj_->serial == obj_->serial);
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

bool EItemRefObjImp::isLessThan(const BObjectImp& objimp) const
{
	if (objimp.isa( BObjectImp::OTApplicObj ))
	{
		const BApplicObjBase* aob = explicit_cast<const BApplicObjBase*, const BObjectImp*>(&objimp);

		if (aob->object_type() == &eitemrefobjimp_type)
		{
			const EItemRefObjImp* itemref_imp =
					explicit_cast<const EItemRefObjImp*,const BApplicObjBase*>(aob);

			return (itemref_imp->obj_->serial < obj_->serial);
		}
		else
		{
			return (&eitemrefobjimp_type < aob->object_type());
		}
	}
	else
	{
		return false;
	}
}

const char* EUBoatRefObjImp::typeOf() const
{
	return "BoatRef";
}

BObjectImp* EUBoatRefObjImp::copy() const
{
	return new EUBoatRefObjImp( obj_.get() );
}

BObjectRef EUBoatRefObjImp::get_member_id( const int id ) //id test
{
	BObjectImp* result = obj_->get_script_member_id( id );
	if (result != NULL)
		return BObjectRef(result);
	else
		return BObjectRef(UninitObject::create());
}

BObjectRef EUBoatRefObjImp::get_member( const char* membername )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->get_member_id(objmember->id);
	else
		return BObjectRef(UninitObject::create());
}

BObjectRef EUBoatRefObjImp::set_member_id( const int id, BObjectImp* value ) //test id
{
	BObjectImp* result = NULL;
	if (value->isa( BObjectImp::OTLong ))
	{
		BLong* lng = static_cast<BLong*>(value);
		result = obj_->set_script_member_id( id, lng->value() );
	}
	else if (value->isa( BObjectImp::OTString ))
	{
		String* str = static_cast<String*>(value);
		result = obj_->set_script_member_id( id, str->value() );
	}
	else if (value->isa( BObjectImp::OTDouble ))
	{
		Double* dbl = static_cast<Double*>(value);
		result = obj_->set_script_member_id_double( id, dbl->value() );
	}
	if (result != NULL)
	{
		return BObjectRef( result );
	}
	else
	{
		return BObjectRef(UninitObject::create());
	}
}

BObjectRef EUBoatRefObjImp::set_member( const char* membername, BObjectImp* value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_member_id(objmember->id, value);
	else
		return BObjectRef(UninitObject::create());
}

BObjectImp* EUBoatRefObjImp::call_method_id( const int id, Executor& ex )
{
	if (!obj_->orphan())
	{
		BObjectImp* imp = obj_->script_method_id( id, ex );
		if (imp != NULL)
			return imp;
		else
			return base::call_method_id( id, ex );
	}
	else
	{
		return new BError( "That object no longer exists" );
	}
}

BObjectImp* EUBoatRefObjImp::call_method( const char* methodname, Executor& ex )
{
	ObjMethod* objmethod = getKnownObjMethod(methodname);
	if ( objmethod != NULL )
		return this->call_method_id(objmethod->id, ex);
	else
		return base::call_method(methodname, ex);
}

bool EUBoatRefObjImp::isTrue() const
{
	return (!obj_->orphan());
}

//dave added this 3/24/3, multi.isa was not being called.
BObjectImp* EMultiRefObjImp::call_method( const char* methodname, Executor& ex )
{
	ObjMethod* objmethod = getKnownObjMethod(methodname);
	if ( objmethod != NULL )
		return this->call_method_id(objmethod->id, ex);
	else
		return base::call_method( methodname, ex );
}

BObjectImp* EMultiRefObjImp::call_method_id( const int id, Executor& ex )
{
	UMulti* multi = obj_.get();
	if (!multi->orphan())
	{
		ObjMethod* mth = getObjMethod(id);
		if ( mth->overridden )
		{
			BObjectImp* imp = multi->custom_script_method( mth->code, ex );
			if (imp)
				return imp;
		}

		BObjectImp* imp = multi->script_method_id( id, ex );
		if (imp != NULL)
			return imp;
		else
			return base::call_method_id( id, ex );
	}
	else
	{
		return new BError( "That object no longer exists" );
	}
}

const char* EMultiRefObjImp::typeOf() const
{
	return "MultiRef";
}

BObjectImp* EMultiRefObjImp::copy() const
{
	return new EMultiRefObjImp( obj_.get() );
}

BObjectRef EMultiRefObjImp::get_member_id( const int id ) //id test
{
	BObjectImp* result = obj_->get_script_member_id( id );
	if (result != NULL)
		return BObjectRef(result);
	else
		return BObjectRef(UninitObject::create());
}

BObjectRef EMultiRefObjImp::get_member( const char* membername )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->get_member_id(objmember->id);
	else
		return BObjectRef(UninitObject::create());
}

BObjectRef EMultiRefObjImp::set_member_id( const int id, BObjectImp* value ) //test id
{
	BObjectImp* result = NULL;
	if (value->isa( BObjectImp::OTLong ))
	{
		BLong* lng = static_cast<BLong*>(value);
		result = obj_->set_script_member_id( id, lng->value() );
	}
	else if (value->isa( BObjectImp::OTString ))
	{
		String* str = static_cast<String*>(value);
		result = obj_->set_script_member_id( id, str->value() );
	}
	else if (value->isa( BObjectImp::OTDouble ))
	{
		Double* dbl = static_cast<Double*>(value);
		result = obj_->set_script_member_id_double( id, dbl->value() );
	}
	if (result != NULL)
	{
		return BObjectRef( result );
	}
	else
	{
		return BObjectRef(UninitObject::create());
	}
}
BObjectRef EMultiRefObjImp::set_member( const char* membername, BObjectImp* value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_member_id(objmember->id, value);
	else
		return BObjectRef(UninitObject::create());
}

bool EMultiRefObjImp::isTrue() const
{
	return (!obj_->orphan());
}

#define GET_STRING_MEMBER(v) if (stricmp( membername, #v ) == 0) return new String( v )
#define SET_STRING_MEMBER(v) if (stricmp( membername, #v ) == 0) return new String( v = value )

#define GET_STRING_MEMBER_(v) if (stricmp( membername, #v ) == 0) return new String( v##_ )
#define SET_STRING_MEMBER_(v) if (stricmp( membername, #v ) == 0) return new String( v##_ = value )

#define GET_SHORT_MEMBER_(v) if (stricmp( membername, #v ) == 0) return new BLong( v##_ )
#define SET_SHORT_MEMBER_(v) if (stricmp( membername, #v ) == 0) return new BLong( v##_ = static_cast<short>(value) )

#define GET_SHORT_MEMBER(v) if (stricmp( membername, #v ) == 0) return new BLong( v )
#define SET_SHORT_MEMBER(v) if (stricmp( membername, #v ) == 0) return new BLong( v = static_cast<short>(value) )

#define GET_LONG_MEMBER(v) if (stricmp( membername, #v ) == 0) return new BLong( v )
#define SET_LONG_MEMBER(v) if (stricmp( membername, #v ) == 0) return new BLong( v = value )

#define GET_LONG_MEMBER_(v) if (stricmp( membername, #v ) == 0) return new BLong( v##_ )
#define SET_LONG_MEMBER_(v) if (stricmp( membername, #v ) == 0) return new BLong( v##_ = value )

#define GET_BOOL_MEMBER_(v) if (stricmp( membername, #v ) == 0) return new BLong( v##_ ? 1 : 0)
#define SET_BOOL_MEMBER_(v) if (stricmp( membername, #v ) == 0) return new BLong( v##_ = value?true:false )

#define GET_BOOL_MEMBER(v) if (stricmp( membername, #v ) == 0) return new BLong( v ? 1 : 0)
#define SET_BOOL_MEMBER(v) if (stricmp( membername, #v ) == 0) return new BLong( v = value?true:false )

#define GET_USHORT_MEMBER_(v) if (stricmp( membername, #v ) == 0) return new BLong( v##_ )
#define SET_USHORT_MEMBER_(v) if (stricmp( membername, #v ) == 0) return new BLong( v##_ = static_cast<unsigned short>(value) )

#define GET_USHORT_MEMBER(v) if (stricmp( membername, #v ) == 0) return new BLong( v )
#define SET_USHORT_MEMBER(v) if (stricmp( membername, #v ) == 0) return new BLong( v = static_cast<unsigned short>(value) )

#define GET_DOUBLE_MEMBER_(v) if (stricmp( membername, #v ) == 0) return new Double( v##_ )
#define SET_DOUBLE_MEMBER_(v) if (stricmp( membername, #v ) == 0) return new Double( v##_ = static_cast<double>(value) )

///id test
BObjectImp* UObject::get_script_member_id( const int id ) const
{
	if (orphan())
		return new UninitObject;
	switch(id)
	{
		case MBR_X: return new BLong(x); break;
		case MBR_Y: return new BLong(y); break;
		case MBR_Z: return new BLong(z); break;
		case MBR_NAME: return new String(name_); break;
		case MBR_OBJTYPE: return new BLong(objtype_); break;
		case MBR_GRAPHIC: return new BLong(graphic); break;
		case MBR_SERIAL: return new BLong(serial); break;
		case MBR_COLOR: return new BLong(color); break;
		case MBR_HEIGHT: return new BLong(height); break;
		case MBR_FACING: return new BLong(facing); break;
		case MBR_DIRTY: return new BLong(dirty_ ? 1 : 0); break;
		case MBR_WEIGHT: return new BLong(weight()); break;
		case MBR_MULTI:
			UMulti* multi;
			if( NULL != (multi = realm->find_supporting_multi(x,y,z)) ) //dave changed this 1/12/3 - wasn't finding supporting multi if item was not in a walkable location (like on a dyn table)
				return multi->make_ref();
			else
				return new BLong(0);
			break;
		case MBR_REALM: 
			if ( realm != NULL )
				return new String(realm->name());
			else
				return new BError("object does not belong to a realm.");
			break;
		default: return NULL;
	}
}
///

BObjectImp* UObject::get_script_member( const char *membername ) const
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->get_script_member_id(objmember->id);
	else
		return NULL;
}

BObjectImp* UObject::set_script_member_id( const int id, const string& value )  //test id
{
	if (orphan())
		return new UninitObject;

	set_dirty();
	switch(id)
	{
		case MBR_NAME:
			if (ismobile() && (value.empty() || isspace(value[0])) )
				return new BError( "mobile.name must not be empty, and must not begin with a space" );
			setname( value );
			return new String( name() );
			break;
		default: return NULL;
	}
}

BObjectImp* UObject::set_script_member( const char *membername, const string& value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_script_member_id(objmember->id, value);
	else
		return NULL;
}

BObjectImp* UObject::set_script_member_id( const int id, long value ) //test id
{
	if (orphan())
		return new UninitObject;

	set_dirty();
	bool res;
	switch(id)
	{
		case MBR_GRAPHIC:
			setgraphic( static_cast<unsigned short>(value) );
			return new BLong( graphic );
		case MBR_COLOR:
			res = setcolor( static_cast<unsigned short>(value) );
			on_color_changed();
			if (!res) // TODO log?
				return new BError( "Invalid color value " + hexint(value) );
			else
				return new BLong( color );
		case MBR_FACING:
			setfacing( static_cast<unsigned char>(value) );
			on_facing_changed();
			return new BLong( facing );
		default: return NULL;
	}
}

BObjectImp* UObject::set_script_member( const char *membername, long value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_script_member_id(objmember->id, value);
	else
		return NULL;
}

BObjectImp* UObject::set_script_member_id_double( const int id, double value ) //test id
{
	set_dirty();
	return NULL;
}

BObjectImp* UObject::set_script_member_double( const char *membername, double value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_script_member_id_double(objmember->id, value);
	else
		return NULL;
}

BObjectImp* Item::get_script_member_id( const int id ) const //id test
{
	BObjectImp* imp = base::get_script_member_id( id );
	if (imp != NULL)
		return imp;

	switch(id)
	{
		case MBR_AMOUNT: return new BLong(amount_); break;
		case MBR_LAYER: return new BLong(layer); break;
		case MBR_CONTAINER:
			if (container != NULL)
				return container->make_ref();
			else
				return new BLong(0);
			break;
		case MBR_USESCRIPT: return new String( on_use_script_ ); break;
		case MBR_EQUIPSCRIPT: return new String( equip_script_ ); break;
		case MBR_UNEQUIPSCRIPT: return new String( unequip_script_ ); break;
		case MBR_DESC: return new String( description() ); break;
		case MBR_MOVABLE: return new BLong( movable_ ? 1 : 0); break;
		case MBR_INVISIBLE: return new BLong( invisible_ ? 1 : 0); break;
		case MBR_DECAYAT: return new BLong( decayat_gameclock_ ); break;
		case MBR_SELLPRICE: return new BLong(sellprice()); break;
		case MBR_BUYPRICE: return new BLong(buyprice()); break;
		case MBR_NEWBIE: return new BLong( newbie_ ? 1 : 0); break;
		case MBR_ITEM_COUNT: return new BLong( item_count() ); break;
		default: return NULL;
	}
}

BObjectImp* Item::get_script_member( const char *membername ) const
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->get_script_member_id(objmember->id);
	else
		return NULL;
}

BObjectImp* Item::set_script_member_id( const int id, const string& value ) //id test
{
	BObjectImp* imp = base::set_script_member_id( id, value );
	if (imp != NULL)
		return imp;

	switch(id)
	{
		case MBR_USESCRIPT:
			on_use_script_ = value;
			return new String( value );
		case MBR_EQUIPSCRIPT:
			equip_script_ = value;
			return new String( value );
		case MBR_UNEQUIPSCRIPT:
			unequip_script_ = value;
			return new String( value );
		default: return NULL;
	}
}

BObjectImp* Item::set_script_member( const char *membername, const string& value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_script_member_id(objmember->id, value);
	else
		return NULL;
}

BObjectImp* Item::set_script_member_id( const int id, long value ) //id test
{
	BObjectImp* imp = base::set_script_member_id( id, value );
	if (imp != NULL)
		return imp;

	switch(id)
	{
		case MBR_MOVABLE:
			restart_decay_timer();
			movable_ = value?true:false;
			increv();
			on_color_changed();
			return new BLong( movable_ );
		case MBR_INVISIBLE:
			restart_decay_timer();
			invisible_ = value?true:false;
			increv();
			on_invisible_changed();
			return new BLong( invisible_ );
		case MBR_DECAYAT:
			decayat_gameclock_ = value;
			return new BLong( decayat_gameclock_ );
		case MBR_SELLPRICE: return new BLong( sellprice_ = value );
		case MBR_BUYPRICE: return new BLong( buyprice_ = value );
		case MBR_NEWBIE: increv(); restart_decay_timer(); return new BLong( newbie_ = value?true:false );
		default: return NULL;
	}
}

BObjectImp* Item::set_script_member( const char *membername, long value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_script_member_id(objmember->id, value);
	else
		return NULL;
}

BObjectImp* Item::custom_script_method( const char* methodname, Executor& ex )
{
	const ItemDesc& id = itemdesc();
	if (id.method_script != NULL)
	{
		unsigned PC;
		if (id.method_script->FindExportedFunction( methodname, ex.numParams()+1, PC ))
		{
			return id.method_script->call( PC, make_ref(), ex.fparams );
		}
	}
	return NULL;
}

BObject Item::call_custom_method( const char* methodname )
{
	BObjectImpRefVec noparams;
	return call_custom_method( methodname, noparams );
}

BObject Item::call_custom_method( const char* methodname, BObjectImpRefVec& pmore )
{
	const ItemDesc& id = itemdesc();
	if (id.method_script != NULL)
	{
		unsigned PC;
		if (id.method_script->FindExportedFunction( methodname, pmore.size()+1, PC ))
		{
			return id.method_script->call( PC, new EItemRefObjImp(this), pmore );
		}
		else
		{
			string message;
			message = "Method script for objtype "
								+ id.objtype_description()
								+ " does not export function "
								+ string(methodname)
								+ " taking "
								+ decint( pmore.size() + 1 )
								+ " parameters";
			BError* err = new BError( message );
			return BObject( err );
		}
	}
	else
	{
		return BObject( new BError( "No method script defined for " + id.objtype_description() ) );
	}
}


class ARUpdater
{
public:
	static void on_change( Character* chr )
	{
		chr->refresh_ar(); // FIXME inefficient
		if (chr->client != NULL)
		{
			send_full_statmsg( chr->client, chr );
		}
	}
};
class HiddenUpdater
{
public:
	static void on_change( Character* chr )
	{
		if (chr->hidden())
		{
			chr->set_stealthsteps(0);
			if (chr->client)
				send_move( chr->client, chr );
			send_remove_character_to_nearby_cantsee( chr );
			send_create_mobile_to_nearby_cansee( chr );
		}
		else
		{
			chr->unhide();
			chr->set_stealthsteps(0);
		}
	}
};


class ConcealedUpdater
{
public:
	static void on_change( Character* chr )
	{
//		chr->check_concealment_level();
		if (chr->concealed())
		{
			if (chr->client)
				send_move( chr->client, chr );
			send_remove_character_to_nearby_cantsee( chr );
			send_create_mobile_to_nearby_cansee( chr );
		}
		else if (chr->is_visible())
		{
			chr->unhide();
		}
	}
};

BObjectImp* Character::make_ref()
{
	return new ECharacterRefObjImp( this );
}
BObjectImp* Character::make_offline_ref()
{
	return new EOfflineCharacterRefObjImp( this );
}

////id test
BObjectImp* Character::get_script_member_id( const int id ) const
{
	BObjectImp* imp = base::get_script_member_id( id );
	if (imp != NULL)
		return imp;

	Item* bp = NULL;

	switch(id)
	{
		case MBR_WARMODE: return new BLong(warmode); break;
		case MBR_GENDER: return new BLong(gender); break;
		case MBR_RACE: return new BLong(race); break;
		case MBR_TRUEOBJTYPE: return new BLong(trueobjtype); break;
		case MBR_TRUECOLOR: return new BLong(truecolor); break;
		case MBR_AR_MOD: return new BLong(ar_mod_); break;
		case MBR_DELAY_MOD: return new BLong(delay_mod_); break;
		case MBR_HIDDEN: return new BLong(hidden_ ? 1 : 0); break;
		case MBR_CONCEALED: return new BLong(concealed_); break;
		case MBR_FROZEN: return new BLong(frozen_ ? 1 : 0); break;
		case MBR_PARALYZED: return new BLong(paralyzed_ ? 1 : 0); break;
		case MBR_POISONED: return new BLong(poisoned ? 1 : 0); break;
		case MBR_STEALTHSTEPS: return new BLong(stealthsteps_); break;
		case MBR_SQUELCHED: return new BLong( squelched() ? 1 : 0 ); break;
		case MBR_DEAD: return new BLong(dead_); break;
		case MBR_AR: return new BLong(ar_); break;
		case MBR_BACKPACK:
			bp = backpack();
			if (bp != NULL)
				return bp->make_ref();
			else
				return new BError( "That has no backpack" );
			break;
		case MBR_WEAPON:
			if (weapon != NULL)
				return weapon->make_ref();
			else
				return new BLong(0);
			break;
		case MBR_SHIELD:
			if (shield != NULL)
				return shield->make_ref();
			else
				return new BLong(0);
			break;
		case MBR_ACCTNAME:
			if (acct != NULL)
				return new String( acct->name() );
			else
				return new BError( "Not attached to an account" );
			break;

		case MBR_ACCT:
			if (acct != NULL)
				return new AccountObjImp( AccountPtrHolder(acct) );
			else
				return new BError( "Not attached to an account" );
			break;
		case MBR_CMDLEVEL: return new BLong(cmdlevel); break;
		case MBR_CMDLEVELSTR: return new String( cmdlevels2[cmdlevel].name ); break;
		case MBR_CRIMINAL: return new BLong( is_criminal() ? 1 : 0 ); break;
		case MBR_IP:
			if (client != NULL)
				return new String( client->ipaddrAsString() );
			else
				return new String( "" );
			break;
		case MBR_GOLD: return new BLong( gold_carried() ); break;

		case MBR_TITLE_PREFIX: return new String(title_prefix); break;
		case MBR_TITLE_SUFFIX: return new String(title_suffix); break;
		case MBR_TITLE_GUILD: return new String(title_guild); break;
		case MBR_TITLE_RACE: return new String(title_race); break;
		case MBR_UCLANG: return new String(uclang); break;
		case MBR_GUILDID: return new BLong( guildid() ); break;
		case MBR_GUILD:
			if (guild_ != NULL)
				return CreateGuildRefObjImp( guild_ );
			else
				return new BError( "Not a member of a guild" );
			break;


		case MBR_MURDERER: return new BLong( murderer_ ? 1 : 0 ); break;
		case MBR_ATTACHED:
			if (script_ex == NULL)
				return new BError( "No script attached." );
			else
				return new ScriptExObjImp( script_ex );
			break;
		case MBR_CLIENTVERSION:
			if (client != NULL)
				return new String( client->getversion() );
			else
				return new String( "" );
			break;

		case MBR_CLIENTINFO:
			if (client != NULL)
				return client->getclientinfo();
			else
				return new BLong(0);
			break;

		case MBR_CREATEDAT: return new BLong( created_at ); break;

		case MBR_REPORTABLES:  return GetReportables(); break;

		case MBR_OPPONENT:
			if ( opponent_ != NULL )
				return opponent_->make_ref();
			else
				return new BError("Mobile does not have any opponent selected.");
			break;
		case MBR_CONNECTED:
			if ( connected )
				return new BLong(1);
			else
				return new BLong(0);
			break;
		case MBR_TRADING_WITH:
			{
				if ( trading_with != NULL )
					return trading_with->make_ref();
				else
					return new BError("Mobile is not currently trading with anyone.");
			}
			break;
		/* -- Too dangerous to give access to at this time.
		case MBR_TRADE_CONTAINER:
			if ( trading_cont != NULL )
				return trading_cont->make_ref();
			else
				return new BError("Mobile does not have a trade container.");
			break;
		*/
		case MBR_ISUOKR:
			if (client != NULL)
				return new BLong( client->isUOKR ? 1 : 0 );
			else
				return new BLong(0);
			break;
		case MBR_CURSOR:
			if (client != NULL)
				return new BLong( target_cursor_busy() ? 1 : 0 );
			else
				return new BLong(0);
			break;
		case MBR_GUMP:
			if (client != NULL)
				return new BLong( has_active_gump() ? 1 : 0 );
			else
				return new BLong(0);
			break;
		case MBR_PROMPT:
			if (client != NULL)
				return new BLong( has_active_prompt() ? 1 : 0 );
			else
				return new BLong(0);
			break;
		default: return NULL;
	}
}
////

///
/// Mobile Scripting Object Members
///
BObjectImp* Character::get_script_member( const char *membername ) const
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->get_script_member_id(objmember->id);
	else
		return NULL;
}

BObjectImp* Character::set_script_member_id( const int id, const std::string& value ) //id test
{
	BObjectImp* imp = base::set_script_member_id( id, value );
	if (imp != NULL)
		return imp;

	String* ret;
	switch(id)
	{
		case MBR_TITLE_PREFIX: ret = new String( title_prefix = value ); break;
		case MBR_TITLE_SUFFIX: ret = new String( title_suffix = value ); break;
		case MBR_TITLE_GUILD: ret = new String( title_guild = value ); break;
		case MBR_TITLE_RACE: ret = new String( title_race = value ); break;
		default: return NULL;
	}
	return ret;
}

BObjectImp* Character::set_script_member( const char *membername, const std::string& value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_script_member_id(objmember->id, value);
	else
		return NULL;
}

BObjectImp* Character::set_script_member_id( const int id, long value ) //id test
{
	BObjectImp* imp = base::set_script_member_id( id, value );
	if (imp != NULL)
		return imp;

	bool oldhidden;
	switch(id)
	{
		case MBR_GENDER:
			if (value)
				gender = GENDER_FEMALE;
			else
				gender = GENDER_MALE;
			return new BLong( gender );
		case MBR_RACE:
			if (value == RACE_ELF)
				race = RACE_ELF;
			else
				race = RACE_HUMAN;
			return new BLong( race );
		case MBR_TRUEOBJTYPE: return new BLong( trueobjtype = static_cast<unsigned short>(value) );
		case MBR_TRUECOLOR:  return new BLong( truecolor = static_cast<unsigned short>(value) );
		case MBR_AR_MOD:
			ar_mod_ = static_cast<short>(value);
			refresh_ar();
			if (client != NULL)
				send_full_statmsg( client, this );
			return new BLong( ar_mod_ );
		case MBR_DELAY_MOD: return new BLong( delay_mod_ = static_cast<short>(value) );
		case MBR_HIDDEN:
			//dave 1/15/3 don't call on_change unless the value actually changed?
			oldhidden = hidden_;
			hidden_ = value?true:false;
			if(oldhidden != hidden_)
				HiddenUpdater::on_change( this );
			return new BLong( hidden_ );
		case MBR_CONCEALED:
			concealed_ = static_cast<unsigned char>(value);
			ConcealedUpdater::on_change( this );
			return new BLong( concealed_ );
		case MBR_FROZEN: return new BLong( frozen_ = value?true:false );
		case MBR_PARALYZED: return new BLong( paralyzed_ = value?true:false );
		case MBR_POISONED:
			poisoned = value?true:false;
			on_poison_changed(); // dave 12/24
			return new BLong( poisoned );
		case MBR_STEALTHSTEPS: return new BLong( stealthsteps_ = static_cast<unsigned short>(value) );
		case MBR_CMDLEVEL:
			cmdlevel = static_cast<unsigned char>(value);
			if (cmdlevel >= cmdlevels2.size())
				cmdlevel = cmdlevels2.size()-1;
			return new BLong( cmdlevel );
		case MBR_MURDERER: return new BLong( murderer_ = value?true:false );
		default: return NULL;
	}
}

BObjectImp* Character::set_script_member( const char *membername, long value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_script_member_id(objmember->id, value);
	else
		return NULL;
}

BObjectImp* Character::script_method_id( const int id, Executor& ex )
{
	BObjectImp* imp = base::script_method_id( id, ex );
	if (imp != NULL)
		return imp;

	bool newval = true;
	long lnewval = 1;
	bool changed;
	long level, duration;
	const String* pstr;
	long amt;
	long serial;
	long gameclock;
	switch(id)
	{
		///
		/// mobile.SetPoisoned( ispoisoned := 1 )
		///	 If the poisoned flag was changed, and the script has a controller
		///		 If poisoned was SET,
		///			 apply RepSystem rules (Mobile damages Mobile)
		///		 else poisoned was CLEARED, so
		///			 apply RepSystem rules (Mobile helps Mobile)
		///
	case MTH_SETPOISONED:
		if (ex.hasParams(1))
		{
			long lval;
			if (!ex.getParam( 0, lval ))
				return new BError( "Invalid parameter type" );
			if (!lval)
				newval = false;
		}
		changed = (newval != poisoned);

		if (changed)
		{
			set_dirty();
			poisoned = newval;
			check_undamaged();
			UOExecutorModule* uoexec = static_cast<UOExecutorModule*>(ex.findModule( "UO" ));
			if (uoexec && uoexec->controller_.get())
			{
				Character* attacker = uoexec->controller_.get();
				if (!attacker->orphan())
				{
					if (poisoned)
						attacker->repsys_on_damage( this );
					else
						attacker->repsys_on_help( this );
				}
			}
			on_poison_changed(); //dave 12-24
		}

		return new BLong(1);

		///
		/// mobile.SetParalyzed( isparalyzed := 1 )
		///	 If the paralyzed flag was changed, and the script has a controller
		///		 if paralyzed was SET,
		///			 apply RepSystem rules (Mobile damages Mobile)
		///		 else paralyzed was CLEARED, so
		///			 apply RepSystem rules (Mobile heals Mobile)
		///
	case MTH_SETPARALYZED:
		newval = true;
		if (ex.hasParams(1))
		{
			long lval;
			if (!ex.getParam( 0, lval ))
				return new BError( "Invalid parameter type" );
			if (!lval)
				newval = false;
		}
		changed = (newval != paralyzed_);

		if (changed)
		{
			set_dirty();
			paralyzed_ = newval;
			check_undamaged();
			UOExecutorModule* uoexec = static_cast<UOExecutorModule*>(ex.findModule( "UO" ));
			if (uoexec && uoexec->controller_.get())
			{
				Character* attacker = uoexec->controller_.get();
				if (!attacker->orphan())
				{
					if (paralyzed_)
						attacker->repsys_on_damage( this );
					else
						attacker->repsys_on_help( this );
				}
			}
		}

		return new BLong(1);

		///
		/// mobile.SetCriminal( level := 1 )
		///   if level is 0, clears the CriminalTimer
		///
	case MTH_SETCRIMINAL:
		level = 1;
		if (ex.hasParams(1))
		{
			if (!ex.getParam( 0, level ))
				return new BError( "Invalid parameter type" );
			if (level < 0)
				return new BError( "Level must be >= 0" );
		}
		set_dirty();
		make_criminal( level );
		return new BLong(1);

	case MTH_SETLIGHTLEVEL:
		if (!ex.hasParams(2))
			return new BError( "Not enough parameters" );
		if (ex.getParam( 0, level ) &&
			ex.getParam( 1, duration ))
		{
			// cout << "Setting light override to " << level << " for " << duration << " seconds." << endl;
			lightoverride = level;
			lightoverride_until = read_gameclock() + duration;
			check_region_changes();
			return new BLong(1);
		}

	case MTH_SQUELCH:
		if (!ex.hasParams(1))
			return new BError( "Not enough parameters" );
		if (ex.getParam( 0, duration ))
		{
			set_dirty();
			if (duration == -1)
				squelched_until = ~0u;
			else if (duration == 0)
				squelched_until = 0;
			else
				squelched_until = read_gameclock() + duration;
			return new BLong(1);
		}
		break;
	case MTH_ENABLE:
		if (!ex.hasParams(1))
			return new BError( "Not enough parameters" );
		if (ex.getStringParam( 0, pstr ))
		{
			if (has_privilege( pstr->data() ))
			{
				set_dirty();
				set_setting( pstr->data(), true );
				return new BLong(1);
			}
			else
			{
				return new BError( "Mobile doesn't have that privilege" );
			}
		}

	case MTH_DISABLE:
		if (!ex.hasParams(1))
			return new BError( "Not enough parameters" );
		if (ex.getStringParam( 0, pstr ))
		{
			set_dirty();
			set_setting( pstr->data(), false );
			return new BLong(1);
		}

	case MTH_ENABLED:
		if (!ex.hasParams(1))
			return new BError( "Not enough parameters" );
		if (ex.getStringParam( 0, pstr ))
		{
			return new BLong( setting_enabled( pstr->data() ) ? 1 : 0);
		}

	case MTH_PRIVILEGES:
	{
		BDictionary* dict = new BDictionary;
		ISTRINGSTREAM istrm(all_privs());
		string tmp;
		while ( istrm >> tmp )
		{
			dict->addMember(new String (tmp), new BLong(setting_enabled(tmp.c_str())));
		}
		return dict;
		break;
	}

	case MTH_SETCMDLEVEL:
		if (!ex.hasParams(1))
			return new BError( "Not enough parameters" );
		if (ex.getStringParam( 0, pstr ))
		{
			CmdLevel* pcmdlevel = find_cmdlevel( pstr->data() );
			if (pcmdlevel)
			{
				set_dirty();
				cmdlevel = pcmdlevel->cmdlevel;
				return new BLong(1);
			}
			else
			{
				return new BError( "No such command level" );
			}
		}
		break;
		/// mobile.SpendGold( amount )
		///	 if mobile has 'amount' gold, spends it.
		///	 otherwise, spends nothing and returns an error("Insufficient funds")
		///
	case MTH_SPENDGOLD:
		if (ex.numParams()!=1 ||
			!ex.getParam( 0, amt ))
			return new BError( "Invalid parameter type" );

		if (gold_carried() < static_cast<unsigned long>(amt))
			return new BError( "Insufficient funds" );

		spend_gold( amt );
		return new BLong( 1 );

		///
		/// mobile.SetMurderer( ismurderer : boolean = true ) // Set or Clear Murderer Flag
		///
	case MTH_SETMURDERER:
		if (ex.hasParams(1))
		{
			if (!ex.getParam( 0, lnewval ))
				return new BError( "Invalid parameter type" );
		}
		set_dirty();
		make_murderer( lnewval?true:false );
		return new BLong(1);

		///
		/// mobile.RemoveReportable( serial, gameclock )
		///	Remove a killer from this mobile's Reportables list
		///
	case MTH_REMOVEREPORTABLE:
		if (!ex.hasParams(2))
			return new BError( "Not enough parameters" );
		if (ex.getParam( 0, serial ) &&
			ex.getParam( 1, gameclock ))
		{
			set_dirty();
			clear_reportable( serial, gameclock );
			return new BLong(1);
		}
		else
		{
			return new BError( "Invalid parameter type" );
		}
		break;
	case MTH_GETGOTTENITEM:
		if( gotten_item != NULL )
			return new EItemRefObjImp(gotten_item);
		else
			return new BError( "Gotten Item NULL" );
		break;
	case MTH_CLEARGOTTENITEM:
		if( gotten_item != NULL )
		{
			clear_gotten_item();
			return new BLong(1);
		}
		else
			return new BError( "No Gotten Item" );
		break;
	case MTH_SETWARMODE:
		long newmode;
		if (!ex.hasParams(1))
			return new BError( "Not enough parameters" );
		if(ex.getParam(0, newmode,0,1))
		{
			set_warmode( (newmode==0) ? false : true );
			if(client) //dave 12-21, duh don't send warmode change to an NPC!
				send_warmode();
			return new BLong( warmode );
		}
		else
		{
			return new BError("Invalid parameter type");
		}
		break;
	case MTH_GETCORPSE:
		{
		UCorpse* corpse_obj = static_cast<UCorpse*>(system_find_item(last_corpse));
		if ( corpse_obj != NULL && !corpse_obj->orphan() )
			return new EItemRefObjImp(corpse_obj);
		else
			return new BError("No corpse was found.");
		}
		break;
	default:
		// didn't recognize the method name
		return NULL;
	}

	// did recognize the method name, but didn't get parameters out
	return new BError( "Invalid parameter type" );
}


BObjectImp* Character::script_method( const char* methodname, Executor& ex )
{
	ObjMethod* objmethod = getKnownObjMethod(methodname);
	if ( objmethod != NULL )
		return this->script_method_id(objmethod->id, ex);	
	else
		return NULL;
}

ObjArray* Character::GetReportables() const
{
	ObjArray* arr = new ObjArray;

	for( ReportableList::const_iterator itr = reportable_.begin();
			itr != reportable_.end(); ++itr )
	{
		const reportable_t& rt = (*itr);

		BObjectImp* kmember = NULL;
		Character* killer = system_find_mobile( rt.serial, SYSFIND_SEARCH_OFFLINE_MOBILES );
		if (killer)
		{
			kmember = new EOfflineCharacterRefObjImp(killer);
		}
		else
		{
			kmember = new BError("Mobile not found");
		}

		BStruct* elem = new BStruct;;
		elem->addMember( "serial", new BLong( rt.serial ) );
		elem->addMember( "killer", kmember );
		elem->addMember( "gameclock", new BLong( rt.polclock ) );

		arr->addElement( elem );
	}
	return arr;
}

BObjectImp* NPC::get_script_member_id( const int id ) const //id test
{
	if ( id == MBR_AR )
	{
		//NPC armor retrieval is handled a bit differently because of intrinsic values.
		if ( ar_ == 0 )
			return new BLong(npc_ar_);
		else
			return new BLong(ar_);
	}

	BObjectImp* imp = base::get_script_member_id( id );
	if (imp != NULL)
		return imp;

	Character* master = NULL;
	switch(id)
	{
		case MBR_SCRIPT: return new String(script); break;
		case MBR_NPCTEMPLATE: return new String( template_name ); break;
		case MBR_MASTER:
			master = master_.get();
			if (master && !master->orphan())
				return new ECharacterRefObjImp( master );
			else
				return new BLong(0);
			break;

		case MBR_PROCESS:
			if (ex)
				return new ScriptExObjImp( ex );
			else
				return new BError( "No script running" );
			break;

		case MBR_EVENTMASK:
			if (ex)
				return new BLong( ex->eventmask );
			else
				return new BError( "No script running" );
			break;

		case MBR_SPEECH_COLOR: return new BLong( speech_color_ ); break;
		case MBR_SPEECH_FONT:  return new BLong( speech_font_ ); break;
		case MBR_USE_ADJUSTMENTS:  return new BLong( use_adjustments ? 1 : 0 ); break;
		case MBR_RUN_SPEED: return new BLong( run_speed ); break;
		case MBR_ALIGNMENT:
			return new BLong(this->template_.alignment);
		default: return NULL;
	}
}

BObjectImp* NPC::get_script_member( const char *membername ) const
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->get_script_member_id(objmember->id);
	else
		return NULL;
}

BObjectImp* NPC::set_script_member_id( const int id, const std::string& value )//id test
{
	BObjectImp* imp = base::set_script_member_id( id, value );
	if (imp != NULL)
		return imp;
	switch(id)
	{
		case MBR_SCRIPT: return new String( script = value );
		default: return NULL;
	}
}

BObjectImp* NPC::set_script_member( const char *membername, const std::string& value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_script_member_id(objmember->id, value);
	else
		return NULL;
}

BObjectImp* NPC::set_script_member_id( const int id, long value )//id test
{
	BObjectImp* imp = base::set_script_member_id( id, value );
	if (imp != NULL)
		return imp;
	switch(id)
	{
		case MBR_SPEECH_COLOR: return new BLong( speech_color_ = static_cast<unsigned short>(value) );
		case MBR_SPEECH_FONT: return new BLong( speech_font_ = static_cast<unsigned short>(value) );
		case MBR_USE_ADJUSTMENTS: return new BLong( use_adjustments = value?true:false );
		case MBR_RUN_SPEED: return new BLong( run_speed = static_cast<unsigned short>(value) );
		default: return NULL;
	}
}
BObjectImp* NPC::set_script_member( const char *membername, long value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_script_member_id(objmember->id, value);
	else
		return NULL;
}

BObjectImp* NPC::script_method_id( const int id, Executor& ex )
{
	BObjectImp* imp = base::script_method_id( id, ex );
	if (imp != NULL)
		return imp;

	switch(id)
	{
	case MTH_SETMASTER:
		if (ex.numParams()!=1)
			return new BError( "Not enough parameters" );
		Character* chr;
		set_dirty();
		if (getCharacterParam( ex, 0, chr ))
		{
			master_.set( chr );
			return new BLong(1);
		}
		else
		{
			master_.clear();
			return new BLong(0);
		}
		break;
	default:
		return NULL;
	}
}

BObjectImp* NPC::script_method( const char* methodname, Executor& ex )
{
	ObjMethod* objmethod = getKnownObjMethod(methodname);
	if ( objmethod != NULL )
		return this->script_method_id(objmethod->id, ex);
	else
		return NULL;
}

BObjectImp* Item::make_ref()
{
	return new EItemRefObjImp( this );
}

BObjectImp* ULockable::get_script_member_id( const int id ) const //id test
{
	BObjectImp* imp = Item::get_script_member_id( id );
	if (imp != NULL)
		return imp;

	switch(id)
	{
		case MBR_LOCKED: return new BLong( locked_ ? 1 : 0 ); break;
		default: return NULL;
	}
}

BObjectImp* ULockable::get_script_member( const char *membername ) const
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->get_script_member_id(objmember->id);
	else
		return NULL;
}

BObjectImp* ULockable::set_script_member_id( const int id, long value )//id test
{
	BObjectImp* imp = Item::set_script_member_id( id, value );//Dave changed to "Item::" from "base::"
	if (imp != NULL)
		return imp;
	switch(id)
	{
		case MBR_LOCKED: return new BLong( locked_ = value?true:false );
		default: return NULL;
	}
}
BObjectImp* ULockable::set_script_member( const char *membername, long value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_script_member_id(objmember->id, value);
	else
		return NULL;
}

BObjectImp* UCorpse::get_script_member_id( const int id ) const //id test
{
	BObjectImp* imp = base::get_script_member_id( id );
	if (imp != NULL)
		return imp;

	switch(id)
	{
		case MBR_CORPSETYPE:  return new BLong(corpsetype); break;
		case MBR_OWNERSERIAL:	return new BLong(ownerserial); break;
		default: return NULL;
	}
}

BObjectImp* UCorpse::get_script_member( const char *membername ) const
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->get_script_member_id(objmember->id);
	else
		return NULL;
}

BObjectImp* UBoat::make_ref()
{
	return new EUBoatRefObjImp( this );
}

BObjectImp* UMulti::make_ref()
{
	return new EMultiRefObjImp( this );
}

BObjectImp* UBoat::get_script_member_id( const int id ) const //id test
{
	BObjectImp* imp = base::get_script_member_id( id );
	if (imp != NULL)
		return imp;

	Item* cp = NULL;
	switch(id)
	{
		case MBR_TILLERMAN:
			cp  = components_[0].get();
			if ( cp != NULL)
				return new EItemRefObjImp( cp );
			else
				return new BError( string("This ship doesn't have that component") );
			break;
		case MBR_PORTPLANK:
			cp  = components_[1].get();
			if ( cp != NULL)
				return new EItemRefObjImp( cp );
			else
				return new BError( string("This ship doesn't have that component") );
			break;
		case MBR_STARBOARDPLANK:
			cp  = components_[2].get();
			if ( cp != NULL)
				return new EItemRefObjImp( cp );
			else
				return new BError( string("This ship doesn't have that component") );
			break;
		case MBR_HOLD:
			cp  = components_[3].get();
			if ( cp != NULL)
				return new EItemRefObjImp( cp );
			else
				return new BError( string("This ship doesn't have that component") );
			break;
		case MBR_ITEMS: return items_list(); break;
		case MBR_MOBILES: return mobiles_list(); break;
		case MBR_HAS_OFFLINE_MOBILES:  return new BLong( has_offline_mobiles() ? 1 : 0 ); break;
		default: return NULL;
	}
}

BObjectImp* UBoat::get_script_member( const char* membername ) const
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->get_script_member_id(objmember->id);
	else
		return NULL;
}

BObjectImp* UBoat::script_method_id( const int id, Executor& ex )
{
	BObjectImp* imp = base::script_method_id( id, ex );
	if (imp != NULL)
		return imp;

	switch(id)
	{
	case MTH_MOVE_OFFLINE_MOBILES:
		xcoord x;
		ycoord y;
		zcoord z;
		const String* strrealm;
		
		if (ex.numParams()==3)
		{
		  if (ex.getParam( 0, x ) &&
			  ex.getParam( 1, y ) &&
			  ex.getParam( 2, z, ZCOORD_MIN, ZCOORD_MAX ))
		  {
			if (!realm->valid(x,y,z))
			  return new BError( "Coordinates are out of range" );
			
			set_dirty();
			move_offline_mobiles( x, y, z, realm );
			return new BLong(1);
		  }
		  else
			return new BError( "Invalid parameter type" );
		}
		else
		  if (ex.numParams()==4)
		  {
			if (ex.getParam( 0, x ) &&
				ex.getParam( 1, y ) &&
				ex.getParam( 2, z, ZCOORD_MIN, ZCOORD_MAX ) &&
				ex.getStringParam( 3, strrealm ))
			{
			  Realm* realm = find_realm(strrealm->value());
			  if (!realm)
				return new BError( "Realm not found" );
			  
			  if (!realm->valid(x,y,z))
				return new BError( "Coordinates are out of range" );
			  
			  set_dirty();
			  move_offline_mobiles( x, y, z, realm );
			  return new BLong(1);
			}
			else
			  return new BError( "Invalid parameter type" );
		  }
		  else
			return new BError( "Not enough parameters" );
		break;
	
	default:
		return NULL;
	}
}

BObjectImp* UBoat::script_method( const char* methodname, Executor& ex )
{
	ObjMethod* objmethod = getKnownObjMethod(methodname);
	if ( objmethod != NULL )
		return this->script_method_id(objmethod->id, ex);
	else
		return NULL;
}

BObjectImp* UPlank::get_script_member_id( const int id ) const //id test
{
	switch(id)
	{
		case MBR_MULTI:
			if (boat_.get())
				return new EUBoatRefObjImp( boat_.get() );
			else
				return new BError( "No boat attached" );
			break;
	}
	return base::get_script_member_id( id );
}

/* UObject defines a 'multi' also, so we have to trap that here first */
BObjectImp* UPlank::get_script_member( const char* membername ) const
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->get_script_member_id(objmember->id);
	else
		return base::get_script_member(membername);
}

BObjectImp* Map::get_script_member_id( const int id ) const // id test
{
	BObjectImp* imp = base::get_script_member_id( id );
	if (imp != NULL)
		return imp;

	switch(id)
	{
		case MBR_XEAST: return new BLong(xeast); break;
		case MBR_XWEST: return new BLong(xwest); break;
		case MBR_YNORTH: return new BLong(ynorth); break;
		case MBR_YSOUTH: return new BLong(ysouth); break;
		case MBR_GUMPWIDTH: return new BLong(gumpwidth); break;
		case MBR_GUMPHEIGHT: return new BLong(gumpheight); break;
		default: return NULL;
	}
}

BObjectImp* Map::get_script_member( const char *membername ) const
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->get_script_member_id(objmember->id);
	else
		return NULL;
}

BObjectImp* Map::set_script_member_id( const int id, long value )//id test
{
	BObjectImp* imp = base::set_script_member_id( id, value );
	if (imp != NULL)
		return imp;
	switch(id)
	{
		case MBR_XEAST: return new BLong( xeast = static_cast<unsigned short>(value) );
		case MBR_XWEST: return new BLong( xwest = static_cast<unsigned short>(value) );
		case MBR_YNORTH: return new BLong( ynorth = static_cast<unsigned short>(value) );
		case MBR_YSOUTH: return new BLong( ysouth = static_cast<unsigned short>(value) );
		case MBR_GUMPWIDTH: return new BLong( gumpwidth = static_cast<unsigned short>(value) );
		case MBR_GUMPHEIGHT: return new BLong( gumpheight = static_cast<unsigned short>(value) );
		default: return NULL;
	}
}
BObjectImp* Map::set_script_member( const char *membername, long value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_script_member_id(objmember->id, value);
	else
		return NULL;
}

BObjectImp* UObject::script_method_id( const int id, Executor& ex )
{
	const String* mname;
	BObjectImp* ret;

	switch(id)
	{
	case MTH_ISA:
		if (!ex.hasParams(1))
			return new BError( "Not enough parameters" );
		long isatype;
		if (!ex.getParam(0,isatype))
			return new BError( "Invalid parameter type" );
		return new BLong(script_isa( static_cast<unsigned>(isatype) ));
		break;
	case MTH_SET_MEMBER:
		if (!ex.hasParams(2))
			return new BError( "Not enough parameters" );
		BObjectImp* objimp;
		if (ex.getStringParam(0,mname) &&
			(objimp = ex.getParamImp(1)) )
		{
			if(objimp->isa( BObjectImp::OTLong ) )
			{
				BLong* lng = static_cast<BLong*>(objimp);
				ret = set_script_member(mname->value().c_str(),lng->value());
			}
			else if(objimp->isa( BObjectImp::OTDouble ) )
			{
				Double* dbl = static_cast<Double*>(objimp);
				ret = set_script_member_double(mname->value().c_str(),dbl->value());
			}
			else if(objimp->isa( BObjectImp::OTString ) )
			{
				String* str = static_cast<String*>(objimp);
				ret = set_script_member(mname->value().c_str(),str->value());
			}
			else
				return new BError( "Invalid value type" );

			if(ret != NULL)
				return ret;
			else
			{
				string message = string("Member ") + string(mname->value()) + string(" not found on that object");
				return new BError( message );
			}

		}
		else
			return new BError( "Invalid parameter type" );
		break;
	case MTH_GET_MEMBER:
		if (!ex.hasParams(1))
			return new BError( "Not enough parameters" );

		if (ex.getStringParam(0,mname))
		{
			ret = get_script_member(mname->value().c_str());
			if(ret != NULL)
				return ret;
			else
			{
				string message = string("Member ") + string(mname->value()) + string(" not found on that object");
				return new BError( message );
			}
		}
		else
			return new BError( "Invalid parameter type" );
		break;
	default:
		bool changed = false;
		BObjectImp* imp = CallPropertyListMethod_id( proplist_, id, ex, changed );
		if (changed)
			set_dirty();
		return imp;
	}
}


BObjectImp* UObject::script_method( const char* methodname, Executor& ex )
{
	ObjMethod* objmethod = getKnownObjMethod(methodname);
	if ( objmethod != NULL )
		return this->script_method_id(objmethod->id, ex);
	else
	{
		bool changed = false;
		BObjectImp* imp = CallPropertyListMethod(proplist_, methodname, ex, changed);
		if ( changed )
			set_dirty();

		return imp;
	}
}

BObjectImp* UObject::custom_script_method( const char* methodname, Executor& ex )
{
	ObjMethod* objmethod = getKnownObjMethod(methodname);
	if ( objmethod != NULL )
		return this->script_method_id(objmethod->id, ex);
	else
		return NULL;
}

BObjectImp* UDoor::get_script_member_id( const int id ) const //id test
{
	BObjectImp* imp = ULockable::get_script_member_id( id );//Dave changed to "ULockable::" from "base::"
	if (imp != NULL)
		return imp;

	switch(id)
	{
		case MBR_ISOPEN: return new BLong( is_open() ? 1 : 0 ); break;
		default: return NULL;
	}
}

BObjectImp* UDoor::get_script_member( const char* membername ) const
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->get_script_member_id(objmember->id);
	else
		return NULL;
}

BObjectImp* UDoor::script_method_id( const int id, Executor& ex )
{
	BObjectImp* imp = ULockable::script_method_id( id, ex );//Dave changed to "ULockable::" from "base::"
	if (imp != NULL)
		return imp;

	switch(id)
	{
	case MTH_OPEN:
		open(); break;
	case MTH_CLOSE:
		close(); break;
	case MTH_TOGGLE:
		toggle(); break;
	default:
		return NULL;
	}
	return new BLong(1);
}

BObjectImp* UDoor::script_method( const char* methodname, Executor& ex )
{
	ObjMethod* objmethod = getKnownObjMethod(methodname);
	if ( objmethod != NULL )
		return this->script_method_id(objmethod->id, ex);
	else
		return NULL;
}

BObjectImp* Equipment::get_script_member_id( const int id ) const //id test
{
	BObjectImp* imp = Item::get_script_member_id( id ); //Dave changed to "Item::" from "base::"
	if (imp != NULL)
		return imp;

	switch(id)
	{
		case MBR_QUALITY: return new Double( quality_ ); break;
		case MBR_HP: return new BLong( hp_ ); break;
		case MBR_MAXHP_MOD: return new BLong( maxhp_mod_ ); break;
		case MBR_MAXHP: return new BLong( static_cast<long>(maxhp() * quality_) ); break;
		default: return NULL;
	}
}
BObjectImp* Equipment::get_script_member( const char *membername ) const
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->get_script_member_id(objmember->id);
	else
		return NULL;
}

BObjectImp* Equipment::set_script_member_id( const int id, long value ) //id test
{
	BObjectImp* imp = Item::set_script_member_id( id, value );//Dave changed to "Item::" from "base::"
	if (imp != NULL)
		return imp;
	switch(id)
	{
		case MBR_HP: return new BLong( hp_ = static_cast<unsigned short>(value) );
		case MBR_MAXHP_MOD: return new BLong( maxhp_mod_ = static_cast<unsigned short>(value) );
		default: return NULL;
	}
}
BObjectImp* Equipment::set_script_member( const char *membername, long value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_script_member_id(objmember->id, value);
	else
		return NULL;
}

BObjectImp* Equipment::set_script_member_id_double( const int id, double value ) //id test
{
	BObjectImp* imp = Item::set_script_member_id_double( id, value );//Dave changed to "Item::" from "base::"
	if (imp != NULL)
		return imp;
	switch(id)
	{
		case MBR_QUALITY: return new Double( quality_ = static_cast<double>(value) );
		default: return NULL;
	}
}
BObjectImp* Equipment::set_script_member_double( const char *membername, double value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_script_member_id_double(objmember->id, value);
	else
		return NULL;
}


BObjectImp* UWeapon::get_script_member_id( const int id ) const //id test
{
	BObjectImp* imp = Equipment::get_script_member_id( id );
	if (imp != NULL)
		return imp;

	switch(id)
	{
		case MBR_DMG_MOD: return new BLong( dmg_mod_ ); break;
		case MBR_ATTRIBUTE: return new String( attribute().name ); break;
		case MBR_HITSCRIPT: return new String( hit_script_.relativename( tmpl->pkg ) ); break;
		case MBR_INTRINSIC:
			return new BLong(is_intrinsic()); break;
		default: return NULL;
	}
}
BObjectImp* UWeapon::get_script_member( const char *membername ) const
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->get_script_member_id(objmember->id);
	else
		return NULL;
}

BObjectImp* UWeapon::set_script_member_id( const int id, const std::string& value ) //id test
{
	BObjectImp* imp = Item::set_script_member_id( id, value );
	if (imp != NULL)
		return imp;

	switch(id)
	{
		case MBR_HITSCRIPT: set_hit_script( value ); return new BLong(1);
		default: return NULL;
	}
}
BObjectImp* UWeapon::set_script_member( const char *membername, const std::string& value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_script_member_id(objmember->id, value);
	else
		return NULL;
}

BObjectImp* UWeapon::set_script_member_id( const int id, long value )//id test
{
	if (is_intrinsic())
		return new BError("Cannot alter an instrinsic NPC weapon member values"); //executor won't return this to the script currently.

	BObjectImp* imp = Equipment::set_script_member_id( id, value );//Dave changed to "Equipment::" from "Item::"
	if (imp != NULL)
		return imp;

	switch(id)
	{
		case MBR_DMG_MOD: return new BLong( dmg_mod_ = static_cast<short>(value) );
		default: return NULL;
	}
}

BObjectImp* UWeapon::set_script_member( const char *membername, long value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_script_member_id(objmember->id, value);
	else
		return NULL;
}

BObjectImp* UWeapon::set_script_member_id_double( const int id , double value )//id test
{
	if (is_intrinsic())
		return new BError("Cannot alter an instrinsic NPC weapon member values"); //executor won't return this to the script currently.
	return base::set_script_member_id_double(id, value);
}

BObjectImp* UWeapon::set_script_member_double( const char *membername, double value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_script_member_id_double(objmember->id, value);
	else
		return base::set_script_member_double(membername, value);
}

BObjectImp* UArmor::get_script_member_id( const int id ) const //id test
{
	BObjectImp* imp = Equipment::get_script_member_id( id );//Dave changed to "Equipment::" from "base::"
	if (imp != NULL)
		return imp;

	switch(id)
	{
		case MBR_AR_MOD: return new BLong(ar_mod_); break;
		case MBR_AR: return new BLong( ar() ); break;
		case MBR_AR_BASE: return new BLong( tmpl->ar ); break;
		case MBR_ONHIT_SCRIPT: return new String( onhitscript_.relativename( tmpl->pkg ) ); break;
		default: return NULL;
	}
}

BObjectImp* UArmor::get_script_member( const char *membername ) const
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->get_script_member_id(objmember->id);
	else
		return NULL;
}

BObjectImp* UArmor::set_script_member_id( const int id, const std::string& value ) //id test
{
	BObjectImp* imp = Item::set_script_member_id( id, value );
	if (imp != NULL)
		return imp;
	switch(id)
	{
		case MBR_ONHIT_SCRIPT: set_onhitscript( value ); return new BLong(1);
		default: return NULL;
	}
}

BObjectImp* UArmor::set_script_member( const char *membername, const std::string& value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_script_member_id(objmember->id, value);
	else
		return NULL;
}

BObjectImp* UArmor::set_script_member_id( const int id, long value ) //id test
{
	BObjectImp* imp = Equipment::set_script_member_id( id, value );//Dave changed to "Equipment::" from "Item::"
	if (imp != NULL)
		return imp;

	switch(id)
	{
		case MBR_AR_MOD: return new BLong( ar_mod_ = static_cast<short>(value) );
		default: return NULL;
	}
}

BObjectImp* UArmor::set_script_member( const char *membername, long value )
{
	ObjMember* objmember = getKnownObjMember(membername);
	if ( objmember != NULL )
		return this->set_script_member_id(objmember->id, value);
	else
		return NULL;
}

#include "eventid.h"

SourcedEvent::SourcedEvent( EVENTID type, Character* source )
{
	addMember( "type", new BLong( type ) );
	addMember( "source", new ECharacterRefObjImp( source ) );
}

SpeechEvent::SpeechEvent( Character* speaker, const char* speech )
{
	addMember( "type", new BLong( EVID_SPOKE ) );
	addMember( "source", new ECharacterRefObjImp( speaker ) );
	addMember( "text", new String( speech ) );
}
SpeechEvent::SpeechEvent( Character* speaker, const char* speech, const char* texttype) //DAVE
{
	addMember( "type", new BLong( EVID_SPOKE ) );
	addMember( "source", new ECharacterRefObjImp( speaker ) );
	addMember( "text", new String( speech ) );
	addMember( "texttype", new String(texttype) );
}

UnicodeSpeechEvent::UnicodeSpeechEvent( Character* speaker, const char* speech,
															const u16* wspeech, const char lang[4] )
{
	ObjArray* arr;
	addMember( "type", new BLong( EVID_SPOKE ) );
	addMember( "source", new ECharacterRefObjImp( speaker ) );
	addMember( "text", new String( speech ) );
	unsigned wlen = 0;
	while( wspeech[wlen] != L'\0' )
		++wlen;
	if ( !convertUCtoArray(wspeech, arr, wlen, true) )
		addMember( "uc_text", new BError("Invalid Unicode speech received.") );
	else {
		addMember( "uc_text", arr );
		addMember( "langcode", new String( lang ) );
	}
}
UnicodeSpeechEvent::UnicodeSpeechEvent( Character* speaker, const char* speech, const char* texttype,
															const u16* wspeech, const char lang[4] )
{
	ObjArray* arr;
	addMember( "type", new BLong( EVID_SPOKE ) );
	addMember( "source", new ECharacterRefObjImp( speaker ) );
	addMember( "text", new String( speech ) );
	unsigned wlen = 0;
	while( wspeech[wlen] != L'\0' )
		++wlen;
	if ( !convertUCtoArray(wspeech, arr, wlen, true) )
		addMember( "uc_text", new BError("Invalid Unicode speech received.") );
	else {
		addMember( "uc_text", arr );
		addMember( "langcode", new String( lang ) );
	}
	addMember( "texttype", new String(texttype) );
}

DamageEvent::DamageEvent( Character* source, unsigned short damage )
{
	addMember( "type", new BLong( EVID_DAMAGED ) );

	if (source != NULL)
		addMember( "source", new ECharacterRefObjImp( source ) );
	else
		addMember( "source", new BLong( 0 ) );

	addMember( "damage", new BLong( damage ) );
}

ItemGivenEvent::ItemGivenEvent( Character* chr_givenby, Item* item_given, NPC* chr_givento ) :
	SourcedEvent( EVID_ITEM_GIVEN, chr_givenby ),
	given_by_(NULL)
{
	addMember( "item", new EItemRefObjImp( item_given ) );

	given_time_ = read_gameclock();
	item_.set(item_given);
	cont_.set( item_given->container );
	given_by_.set(chr_givenby);

	BLong* givenby = new BLong( chr_givenby->serial );
	BLong* givento = new BLong( chr_givento->serial );
	BLong* giventime = new BLong( given_time_ );
	item_given->setprop( "GivenBy", givenby->pack() );
	item_given->setprop( "GivenTo", givento->pack() );
	item_given->setprop( "GivenTime", giventime->pack() );

}

ItemGivenEvent::~ItemGivenEvent()
{
	/* See if the item is still in the container it was in
	   This means the AI script didn't do anything with it.
	 */
	Item* item = item_.get();
	UContainer* cont = cont_.get();
	Character* chr = given_by_.get();

	std::string given_time_str;
	if (!item->getprop( "GivenTime", given_time_str ))
		given_time_str = "";

	item->eraseprop( "GivenBy" );
	item->eraseprop( "GivenTo" );
	item->eraseprop( "GivenTime" );

	BObjectImp* given_value = BObjectImp::unpack( given_time_str.c_str() );
	long gts = static_cast<BLong*>(given_value)->value();

	if (item->orphan() || cont->orphan() || chr->orphan())
		return;

	if (item->container == cont && decint( given_time_ ) == decint( gts ))
	{
		// blech, it's still in the container!
		// Try to give it back to the character.
		UContainer* backpack = chr->backpack();
		if (backpack != NULL && !chr->dead())
		{
			if (backpack->can_add( *item ))
			{
				cont->remove( item );
				backpack->add( item );
				update_item_to_inrange( item );
				return;
			}
		}
		cont->remove( item );
		item->x = chr->x;
		item->y = chr->y;
		item->z = chr->z;
		add_item_to_world( item );
		register_with_supporting_multi( item );
		move_item( item, item->x, item->y, item->z, NULL );
	}
}

bool UObject::script_isa( unsigned isatype ) const
{
	return (isatype == POLCLASS_OBJECT);
}

bool Item::script_isa( unsigned isatype ) const
{
	return (isatype == POLCLASS_ITEM) || base::script_isa(isatype);
}

bool Character::script_isa( unsigned isatype ) const
{
	return (isatype == POLCLASS_MOBILE) || base::script_isa(isatype);
}

bool NPC::script_isa( unsigned isatype ) const
{
	return (isatype == POLCLASS_NPC) || base::script_isa(isatype);
}

bool ULockable::script_isa( unsigned isatype ) const
{
	return (isatype == POLCLASS_LOCKABLE) || base::script_isa(isatype);
}

bool UContainer::script_isa( unsigned isatype ) const
{
	return (isatype == POLCLASS_CONTAINER) || base::script_isa(isatype);
}

bool UCorpse::script_isa( unsigned isatype ) const
{
	return (isatype == POLCLASS_CORPSE) || base::script_isa(isatype);
}

bool UDoor::script_isa( unsigned isatype ) const
{
	return (isatype == POLCLASS_DOOR) || base::script_isa(isatype);
}

bool Spellbook::script_isa( unsigned isatype ) const
{
	return (isatype == POLCLASS_SPELLBOOK) || base::script_isa(isatype);
}

bool Map::script_isa( unsigned isatype ) const
{
	return (isatype == POLCLASS_MAP) || base::script_isa(isatype);
}

bool UMulti::script_isa( unsigned isatype ) const
{
	return (isatype == POLCLASS_MULTI) || base::script_isa(isatype);
}

bool UBoat::script_isa( unsigned isatype ) const
{
	return (isatype == POLCLASS_BOAT) || base::script_isa(isatype);
}

bool UHouse::script_isa( unsigned isatype ) const
{
	return (isatype == POLCLASS_HOUSE) || base::script_isa(isatype);
}

bool Equipment::script_isa( unsigned isatype ) const
{
	return (isatype == POLCLASS_EQUIPMENT) || base::script_isa(isatype);
}

bool UArmor::script_isa( unsigned isatype ) const
{
	return (isatype == POLCLASS_ARMOR) || base::script_isa(isatype);
}

bool UWeapon::script_isa( unsigned isatype ) const
{
	return (isatype == POLCLASS_WEAPON) || base::script_isa(isatype);
}

