// History
//   2005/09/16 Shinigami: added scripts_thread_script* to support better debugging
//   2006/05/11 Shinigami: better logging in ::signal_event()
//   2006/05/27 Shinigami: fixed a smaller cout-bug
//   2006/09/17 Shinigami: ::signal_event() will return error on full evene queue
//   2006/09/23 Shinigami: Script_Cycles, Sleep_Cycles and Script_passes uses 64bit now

#include "clib/stl_inc.h"

/*using std::cerr;
using std::cout;
using std::endl;
using std::for_each;
using std::list;
using std::set;
using std::string;
*/
#include <time.h>

#include "bscript/basicio.h"
#include "bscript/basicmod.h"
#include "bscript/berror.h"
#include "bscript/eprog.h"
#include "bscript/executor.h"
#include "bscript/fileemod.h"
#include "bscript/impstr.h"
#include "bscript/mathemod.h"

#include "clib/logfile.h"
#include "clib/endian.h"
#include "clib/passert.h"
#include "clib/stlutil.h"
#include "clib/strutil.h"
#include "clib/unicode.h"

#include "attributemod.h"
#include "boatemod.h"
#include "cfgemod.h"
#include "clemod.h"
#include "charactr.h"
#include "datastore.h"
#include "exscrobj.h"
#include "fileaccess.h"
#include "guilds.h"
#include "logfiles.h"
#include "npc.h"
#include "npcemod.h"
#include "osemod.h"
#include "polcfg.h"
#include "polclock.h"
#include "poldbg.h"
#include "polsig.h"
#include "polsystememod.h"
#include "profile.h"
#include "scrdef.h"
#include "scrstore.h"
#include "storagemod.h"
#include "uniemod.h"
#include "uoemod.h"
#include "uoexec.h"
#include "utilemod.h"
#include "vitalmod.h"
#include "watch.h"

#include "nmsgtype.h"
#include "charactr.h"
#include "client.h"

#include "scrsched.h"

PidList pidlist;
unsigned long next_pid = 0;

unsigned long getnewpid( UOExecutor* uoexec )
{
    while (1)
    {
        unsigned long newpid = ++next_pid;
        if (newpid != 0 && 
            pidlist.find(newpid) == pidlist.end())
        {
            pidlist[newpid] = uoexec;
            return newpid;
        }
    }
}
void freepid( unsigned long pid )
{
    pidlist.erase( pid );
}
bool find_uoexec( unsigned long pid, UOExecutor** pp_uoexec )
{
    std::map<unsigned long, UOExecutor*>::iterator itr = pidlist.find( pid );
    if (itr != pidlist.end())
    {
        *pp_uoexec = (*itr).second;
        return true;
    }
    else
    {
        *pp_uoexec = NULL;
        return false;
    }
}

/* Script Scheduler */

OSExecutorModule::OSExecutorModule( Executor& exec ) :
	ExecutorModule( "OS", exec ),
    critical(false),
    priority(1),
    warn_on_runaway(true),
	blocked_(false),
	sleep_until_clock_(0),
    in_hold_list_(NO_LIST),
    hold_itr_(),
    pid_(getnewpid( static_cast<UOExecutor*>(&exec) )),
	max_eventqueue_size(MAX_EVENTQUEUE_SIZE),
    events_()
{
}

OSExecutorModule::~OSExecutorModule()
{
    freepid( pid_ );
    pid_ = 0;
    while (!events_.empty())
    {
        BObject ob( events_.front() );
        events_.pop();
    }
}

unsigned long OSExecutorModule::pid() const
{
    return pid_;
}

bool OSExecutorModule::blocked() const
{
    return blocked_;
}

OSFunctionDef OSExecutorModule::function_table[] =
{
    { "create_debug_context",       &OSExecutorModule::create_debug_context },
    { "getprocess",                 &OSExecutorModule::getprocess },
    { "get_process",                &OSExecutorModule::getprocess },
    { "getpid",                     &OSExecutorModule::getpid },
    { "sleep",                      &OSExecutorModule::sleep },
    { "sleepms",                    &OSExecutorModule::sleepms },
    { "wait_for_event",             &OSExecutorModule::wait_for_event },
    { "events_waiting",             &OSExecutorModule::events_waiting },
    { "start_script",               &OSExecutorModule::start_script },
    { "set_critical",               &OSExecutorModule::set_critical },
	{ "is_critical",				&OSExecutorModule::is_critical },
	{ "run_script_to_completion",   &OSExecutorModule::run_script_to_completion },
    // { "parameter", mf_parameter },
    { "set_debug",                  &OSExecutorModule::mf_set_debug },
    { "syslog",                     &OSExecutorModule::mf_Log },
    { "system_rpm",                 &OSExecutorModule::mf_system_rpm },
    { "set_priority",               &OSExecutorModule::mf_set_priority },
    { "unload_scripts",             &OSExecutorModule::mf_unload_scripts },
    { "set_script_option",          &OSExecutorModule::mf_set_script_option },
	{ "clear_event_queue",			&OSExecutorModule::mf_clear_event_queue },
	{ "set_event_queue_size",		&OSExecutorModule::mf_set_event_queue_size },
	{ "OpenURL",					&OSExecutorModule::mf_OpenURL }

//    { "register_object",            &OSExecutorModule::mf_register_object },
//    { "unregister_object",          &OSExecutorModule::mf_unregister_object },
//    { "get_registered_object",      &OSExecutorModule::mf_get_registered_object }
};

int OSExecutorModule::functionIndex( const char *name )
{
	for( unsigned idx = 0; idx < arsize(function_table); idx++ )
	{
		if (stricmp( name, function_table[idx].funcname ) == 0)
			return idx;
	}
	return -1;
}

BObjectImp* OSExecutorModule::execFunc( unsigned funcidx )
{
	return callMemberFunction(*this, function_table[funcidx].fptr)();
};

BObjectImp* OSExecutorModule::create_debug_context()
{
    return ::create_debug_context();
}

BObjectImp* OSExecutorModule::getprocess()
{
    long pid;
    if (getParam( 0, pid ))
    {
        UOExecutor* uoexec;
        if (find_uoexec( static_cast<unsigned long>(pid), &uoexec ))
            return new ScriptExObjImp( uoexec );
        else
            return new BError( "Process not found" );

    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* OSExecutorModule::getpid()
{
    return new BLong( pid_ );
}
/*	Ok, this is going to be fun.  In the case where we block,
	the caller is going to take our return value and push
	it on the value stack.  
   
	What we'll do is push the value that should be returned
	if a timeout occurs.  THis way, for timeouts, all we have
	to do is move the script back into the run queue.
	
	When we actually complete something, we'll have to
	pop the value off the stack, and replace it with
	the real result.
	
	Whew!
*/
BObjectImp* OSExecutorModule::sleep()
{
	int nsecs;

	nsecs = (int) exec.paramAsLong(0);

	SleepFor( nsecs );

	return new BLong( 0 );
}

BObjectImp* OSExecutorModule::sleepms()
{
	int msecs;

	msecs = (int) exec.paramAsLong(0);

	SleepForMs( msecs );

	return new BLong( 0 );
}

BObjectImp* OSExecutorModule::wait_for_event()
{
    int nsecs;
    if (!events_.empty())
    {
        BObjectImp* imp = events_.front();
        events_.pop();
        return imp;
    }
    else
    {
        nsecs = (int) exec.paramAsLong(0);

        if (nsecs)
        {
            wait_type = WAIT_EVENT;
            blocked_ = true;
            sleep_until_clock_ = polclock() + nsecs * POLCLOCKS_PER_SEC;
        }
        return new BLong( 0 );
    }
}

BObjectImp* OSExecutorModule::events_waiting()
{
    return new BLong( events_.size() );
}


BObjectImp* OSExecutorModule::set_critical()
{
    long crit;
    if (exec.getParam( 0, crit ))
    {
        critical = (crit != 0);
        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* OSExecutorModule::is_critical()
{
	if(critical)
		return new BLong(1);
	else
		return new BLong(0);
}

BObjectImp* OSExecutorModule::mf_set_priority()
{
    long newpri;
    if (getParam( 0, newpri, 1, 255 ))
    {
        long oldpri = priority;
        priority = static_cast<unsigned char>(newpri);
        return new BLong( oldpri );
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* OSExecutorModule::mf_set_debug()
{
    long dbg;
    if (getParam( 0, dbg ))
    {
        if (dbg)
            exec.setDebugLevel( Executor::SOURCELINES );
        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* OSExecutorModule::mf_Log()
{
    BObjectImp* imp = exec.getParamImp( 0 );
    if (imp->isa( BObjectImp::OTString ))
    {
        String* str = static_cast<String*>(imp);
        Log( "[%s]: %s\n", exec.scriptname().c_str(), str->data() );
        cout << "syslog [" << exec.scriptname() << "]: " << str->data() << endl;
        return new BLong(1);
    }
    else
    {
        std::string strval = imp->getStringRep();
        Log( "[%s]: %s\n", exec.scriptname().c_str(), strval.c_str() );
        cout << "syslog: [" << exec.scriptname() << "]: " << strval << endl;
        return new BLong(1);
    }
}

BObjectImp* OSExecutorModule::mf_system_rpm()
{
    return new BLong( last_rpm );
}

BObjectImp* OSExecutorModule::start_script()
{
	const String* scriptname_str;
    if (exec.getStringParam( 0, scriptname_str ))
    {
        BObjectImp* imp = exec.getParamImp( 1 );

	    // FIXME needs to inherit available modules?
        ScriptDef sd;
        if (!sd.config_nodie( scriptname_str->value(), exec.prog()->pkg, "scripts/" ))
        {
            return new BError( "Error in script name" );
        }
        if (!sd.exists())
        {
            return new BError( "Script " + sd.name() + " does not exist." );
        }
	    UOExecutorModule* new_uoemod = ::start_script( sd, imp->copy() );
        if (new_uoemod == NULL)
        {
            return new BError( "Unable to start script" );
        }
        UOExecutorModule* this_uoemod = static_cast<UOExecutorModule*>(exec.findModule( "uo" ));
        if (new_uoemod && this_uoemod)
        {
            new_uoemod->controller_ = this_uoemod->controller_;
        }
	    UOExecutor* uoexec = static_cast<UOExecutor*>(&new_uoemod->exec);
        return new ScriptExObjImp( uoexec );
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* OSExecutorModule::run_script_to_completion()
{
	const char* scriptname = exec.paramAsString(0);
	if (scriptname == NULL)
		return new BLong(0);

	// FIXME needs to inherit available modules?
    ScriptDef script;
    
    if (!script.config_nodie( scriptname, exec.prog()->pkg, "scripts/" ))
        return new BError( "Script descriptor error" );

    if (!script.exists())
        return new BError( "Script does not exist" );

    return ::run_script_to_completion( script, getParamImp(1) ); 
}

BObjectImp* OSExecutorModule::mf_unload_scripts()
{
    const String* str;
    if (getStringParam( 0, str ))
    {
        int n;
        if (str->length() == 0)
            n = unload_all_scripts();
        else
            n = unload_script( str->data() );
        return new BLong(n);
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

const int SCRIPTOPT_NO_INTERRUPT    = 1;
const int SCRIPTOPT_DEBUG           = 2;
const int SCRIPTOPT_NO_RUNAWAY      = 3;
const int SCRIPTOPT_CAN_ACCESS_OFFLINE_MOBILES = 4;

BObjectImp* OSExecutorModule::mf_set_script_option()
{
    long optnum;
    long optval;
    if (getParam( 0, optnum ) && getParam( 1, optval ))
    {
        switch( optnum )
        {
            case SCRIPTOPT_NO_INTERRUPT:
                critical = optval?true:false; 
                break;
            case SCRIPTOPT_DEBUG:
                if (optval) exec.setDebugLevel( Executor::SOURCELINES );
                break;
            case SCRIPTOPT_NO_RUNAWAY:
                warn_on_runaway = !optval;
                break;
            case SCRIPTOPT_CAN_ACCESS_OFFLINE_MOBILES:
                {
                    UOExecutor& uoexec = static_cast<UOExecutor&>(exec);
                    uoexec.can_access_offline_mobiles = optval?true:false;
                }
                break;
            default:
                return new BError( "Unknown Script Option" );
        }
        return new BLong(1);
    }
    else
    {
        return new BError( "Invalid parameter type" );
    }
}

BObjectImp* OSExecutorModule::mf_clear_event_queue() //DAVE
{ 
    return( clear_event_queue() );
}

int max_eventqueue_size = 0;

BObjectImp* OSExecutorModule::clear_event_queue() //DAVE
{
    while (!events_.empty())
    {
        BObject ob( events_.front() );
        events_.pop();
    }
	return new BLong(1);
}

BObjectImp* OSExecutorModule::mf_set_event_queue_size() //DAVE 11/24
{
	unsigned short param;
	if (getParam( 0, param ))
	{
		unsigned short oldsize = max_eventqueue_size;
		max_eventqueue_size = param;
		return new BLong(oldsize);
	}
	else
		return new BError( "Invalid parameter type" );
}

bool OSExecutorModule::signal_event( BObjectImp* imp )
{
    INC_PROFILEVAR(events);

    if (blocked_ && (wait_type == WAIT_EVENT)) // already being waited for
    {
        /* Now, the tricky part.  The value to return on an error or
		   completion condition has already been pushed onto the value
		   stack - so, we need to replace it with the real result.
        */
        exec.ValueStack.top().set( new BObject( imp ) );
        /* This executor will get moved to the run queue at the
           next step_scripts(), where blocked is checked looking
           for timed out or completed operations. */

        revive();
    }
    else        // not being waited for, so queue for later.
    {
        if (events_.size() < max_eventqueue_size)
        {
            events_.push( imp );
        }
        else
        {
            cout << "Event queue for " << exec.scriptname() << " is full, discarding event." << endl;
            ExecutorModule* em = exec.findModule( "npc" );
            if (em)
            {
                NPCExecutorModule* npcemod = static_cast<NPCExecutorModule*>(em);
                cout << "NPC Serial: " << hexint( npcemod->npc.serial ) <<
					" (" << npcemod->npc.x << " " << npcemod->npc.y << " " << (int) npcemod->npc.z << ")" << endl;
            }
            BObject ob( imp );
            cout << "Event: " << ob << endl;

			return false;
        }
    }

	return true;
}

void OSExecutorModule::SleepFor( int nsecs )
{
    if (nsecs)
    {
	    blocked_ = true;
        wait_type = WAIT_SLEEP;
	    sleep_until_clock_ = polclock() + nsecs * POLCLOCKS_PER_SEC;
    }
}

void OSExecutorModule::SleepForMs( int msecs )
{
    if (msecs)
    {
	    blocked_ = true;
        wait_type = WAIT_SLEEP;
	    sleep_until_clock_ = polclock() + msecs * POLCLOCKS_PER_SEC / 1000;
    }
}

void OSExecutorModule::suspend()
{
    blocked_ = true;
    wait_type = WAIT_SLEEP;
    sleep_until_clock_ = 0; // wait forever
}

void OSExecutorModule::revive()
{
    blocked_ = false;
    if (in_hold_list_ == TIMEOUT_LIST)
    {
        holdlist.erase( hold_itr_ );
        in_hold_list_ = NO_LIST;
        runlist.push_back( static_cast<UOExecutor*>(&exec) );
    }
    else if (in_hold_list_ == NOTIMEOUT_LIST)
    {
        notimeoutholdlist.erase( static_cast<UOExecutor*>(&exec) );
        in_hold_list_ = NO_LIST;
        runlist.push_back( static_cast<UOExecutor*>(&exec) );
    }
    else if (in_hold_list_ == DEBUGGER_LIST)
    {
        // stays right where it is.
    }
}
bool OSExecutorModule::in_debugger_holdlist() const
{
    return (in_hold_list_ == DEBUGGER_LIST);
}
void OSExecutorModule::revive_debugged()
{
    debuggerholdlist.erase( static_cast<UOExecutor*>(&exec) );
    in_hold_list_ = NO_LIST;
    runlist.push_back( static_cast<UOExecutor*>(&exec) );
}

UOExecutor::UOExecutor( ) :
    Executor(std::cerr),
	os_module(NULL),
    instr_cycles(0),
    sleep_cycles(0),
    start_time(poltime()),
    warn_runaway_on_cycle(config.runaway_script_threshold),
    runaway_cycles(0),
    eventmask(0),
    area_size(0),
    speech_size(1),
    can_access_offline_mobiles(false)
{
    weakptr.set( this );
	os_module = new OSExecutorModule( *this );
	addModule( os_module );
}

UOExecutor::~UOExecutor()
{
	// note, the os_module isn't deleted here because
	// the Executor deletes its ExecutorModules.
    if ((instr_cycles >= 500) && watch.profile_scripts)
    {
        long elapsed = poltime() - start_time; // Doh! A script can't run more than 68 years, for this to work.
        Log( "Script %s: %"OUT64"d instr cycles, %"OUT64"d sleep cycles, %ld seconds\n",
                scriptname().c_str(), instr_cycles, sleep_cycles, elapsed );
        cerr << "Script " << scriptname() << ": "
                << instr_cycles << " instr cycles, "
                << sleep_cycles << " sleep cycles, " 
                << elapsed << " seconds elapsed." 
                << endl;
    }
}

std::string UOExecutor::state()
{
    if (halt())
        return "Debugging";
    else if (os_module->blocked())
        return "Sleeping";
    else
        return "Running";
}


// Note, when the program exits, each executor in these queues
// will be deleted by cleanup_scripts()
// Therefore, any object that owns an executor must be destroyed 
// before cleanup_scripts() is called.

ExecList runlist;
ExecList ranlist;


HoldList holdlist;
NoTimeoutHoldList notimeoutholdlist;
NoTimeoutHoldList debuggerholdlist;

int priority_divide = 1;

void cleanup_scripts()
{
    delete_all( runlist );
    while (!holdlist.empty())
    {
        delete ((*holdlist.begin()).second);
        holdlist.erase( holdlist.begin() );
    }
    while (!notimeoutholdlist.empty())
    {
        delete (*notimeoutholdlist.begin());
        notimeoutholdlist.erase( notimeoutholdlist.begin() );
    }
    while (!debuggerholdlist.empty())
    {
        delete (*debuggerholdlist.begin());
        debuggerholdlist.erase( debuggerholdlist.begin() );
    }
}


void run_ready()
{
    THREAD_CHECKPOINT( scripts, 110 );
    while (!runlist.empty())
    {
        ExecList::iterator itr = runlist.begin();
        UOExecutor* ex = *itr;
        OSExecutorModule* os_module = ex->os_module;
        scripts_thread_script = ex->scriptname();
        int inscount = 0;
        int totcount = 0;
        int insleft = os_module->priority / priority_divide;
        if (insleft == 0)
            insleft = 1;

        THREAD_CHECKPOINT( scripts, 111 );

        while (ex->runnable())
        {
            ++ex->instr_cycles;
            THREAD_CHECKPOINT( scripts, 112 );
            scripts_thread_scriptPC = ex->PC;
            ex->execInstr();

            THREAD_CHECKPOINT( scripts, 113 );

            if (os_module->blocked_)
            {
                ex->warn_runaway_on_cycle = ex->instr_cycles + config.runaway_script_threshold;
                ex->runaway_cycles = 0;
                break;
            }
            
            if (ex->instr_cycles == ex->warn_runaway_on_cycle)
            {
                ex->runaway_cycles += config.runaway_script_threshold;
                if (os_module->warn_on_runaway)
                {
                    script_log << "Runaway script[" << os_module->pid() << "]: " << ex->scriptname() 
                                 << " (" << ex->runaway_cycles << " cycles)" << endl;
                    ex->show_context( script_log, ex->PC );
                }
                ex->warn_runaway_on_cycle += config.runaway_script_threshold;
            }

            if (os_module->critical)
            {
                ++inscount;
                ++totcount;
                if (inscount > 1000)
                {
                    inscount = 0;
					if(config.report_critical_scripts)
						cerr << "Critical script " << ex->scriptname() << " has run for " << totcount << " instructions" << endl;
                }
                continue;
            }

            if (!--insleft)
            {
                break;
            }
        }

         // hmm, this new terminology (runnable()) is confusing
         // in this case.  Technically, something that is blocked
         // isn't runnable.
        if (!ex->runnable())
		{
            if (ex->error() || ex->done)
            {
                THREAD_CHECKPOINT( scripts, 114 );

                runlist.erase( itr );
                delete ex;
                continue;
            }
            else if (!ex->os_module->blocked_)
            {
                THREAD_CHECKPOINT( scripts, 115 );

                runlist.erase( itr );
                ex->os_module->in_hold_list_ = OSExecutorModule::DEBUGGER_LIST;
                debuggerholdlist.insert( ex );
                continue;
            }
		}
		
        if (ex->os_module->blocked_)
		{
            THREAD_CHECKPOINT( scripts, 116 );

            if (ex->os_module->sleep_until_clock_)
            {
                ex->os_module->in_hold_list_ = OSExecutorModule::TIMEOUT_LIST;
                ex->os_module->hold_itr_ = holdlist.insert( HoldList::value_type( ex->os_module->sleep_until_clock_, ex ) );
            }
            else
            {
                ex->os_module->in_hold_list_ = OSExecutorModule::NOTIMEOUT_LIST;
                notimeoutholdlist.insert( ex );
            }

            runlist.erase( itr );
            --ex->sleep_cycles; // it'd get counted twice otherwise
            --sleep_cycles;
            
            THREAD_CHECKPOINT( scripts, 117 );
		}
        else
        {
            ranlist.splice( ranlist.end(), runlist, itr );
        }
    }
    THREAD_CHECKPOINT( scripts, 118 );
    
    runlist.swap( ranlist );
    THREAD_CHECKPOINT( scripts, 119 );
}


void check_blocked( polclock_t* pclocksleft )
{
	polclock_t now_clock = polclock();
    sleep_cycles += holdlist.size() + notimeoutholdlist.size();
    polclock_t clocksleft = POLCLOCKS_PER_SEC * 60;
    while (1)
    {
        THREAD_CHECKPOINT( scripts, 131 );

        HoldList::iterator itr = holdlist.begin();
        if (itr == holdlist.end())
            break;

        UOExecutor* ex = (*itr).second;
        // ++ex->sleep_cycles;

        passert( ex->os_module->blocked_ );
        passert( ex->os_module->sleep_until_clock_ != 0 );
        clocksleft = ex->os_module->sleep_until_clock_ - now_clock;
        if ( clocksleft <= 0 ) 
		{
            if (clocksleft == 0)
                INC_PROFILEVAR( scripts_ontime );
            else
                INC_PROFILEVAR( scripts_late );
			// wakey-wakey
			// read comment above to understand what goes on here.
			// the return value is already on the stack.
            THREAD_CHECKPOINT( scripts, 132 );
			ex->os_module->revive();
		}
        else
        {
            break;
        }
    }
    *pclocksleft = clocksleft;
}

polclock_t calc_script_clocksleft( polclock_t now )
{
    if (!runlist.empty())
    {
        return 0; // we want to run immediately
    }
    else if (!holdlist.empty())
    {
        HoldList::iterator itr = holdlist.begin();
        UOExecutor* ex = (*itr).second;
        polclock_t clocksleft = ex->os_module->sleep_until_clock_ - now;
        if (clocksleft >= 0)
            return clocksleft;
        else
            return 0;
        
    }
    else
    {
        return -1;
    }
}

void step_scripts( polclock_t* clocksleft, bool* pactivity )
{
    // cerr << "r";
    THREAD_CHECKPOINT( scripts, 102 );
    *pactivity = (!runlist.empty());
    THREAD_CHECKPOINT( scripts, 103 );

    run_ready();

    // cerr << "h";
    THREAD_CHECKPOINT( scripts, 104 );

    check_blocked( clocksleft );
    THREAD_CHECKPOINT( scripts, 105 );
    if (!runlist.empty())
        *clocksleft = 0; 
    THREAD_CHECKPOINT( scripts, 106 );
}

void start_script( const char *filename, 
                   BObjectImp* param0,
                   BObjectImp* param1 )
{
    BObject bobj0( param0 ); // just to delete if it doesn't go somewhere else
    BObject bobj1( param1?param1:UninitObject::create() );

    ref_ptr<EScriptProgram> program = find_script( filename );
    if (program.get() == NULL)
	{
		cerr << "Error reading script " << filename << endl;
        throw runtime_error( "Error starting script" );
	}

    UOExecutor* ex = create_script_executor();
    if (program->haveProgram)
    {
        if (param1)
            ex->pushArg( param1 );
        if (param0)
            ex->pushArg( param0 );
    }
	//ex->addModule( new FileExecutorModule( *ex ) );	
	ex->addModule( new UOExecutorModule( *ex ) );	

    if (!ex->setProgram( program.get() ))
        throw runtime_error( "Error starting script." );

    ex->setDebugLevel( Executor::NONE );


	runlist.push_back( ex );
}
// EXACTLY the same as start_script, except uses find_script2
UOExecutorModule* start_script( const ScriptDef& script, BObjectImp* param ) throw()
{
    BObject bobj( param?param:UninitObject::create() ); // just to delete if it doesn't go somewhere else
    ref_ptr<EScriptProgram> program = find_script2( script );
    if (program.get() == NULL)
	{
		cerr << "Error reading script " << script.name() << endl;
        // throw runtime_error( "Error starting script" );
        return NULL;
	}

    auto_ptr<UOExecutor> ex( create_script_executor() );
    if (program->haveProgram && (param != NULL))
    {
        ex->pushArg( param );
    }
	//ex->addModule( new FileExecutorModule( *ex ) );	
	UOExecutorModule* uoemod = new UOExecutorModule( *ex );
    ex->addModule( uoemod );	

    if (!ex->setProgram( program.get() ))
    {
        return NULL;
        //throw runtime_error( "Error starting script." );
    }

    ex->setDebugLevel( Executor::NONE );


	runlist.push_back( ex.release() );

    return uoemod;
}

UOExecutorModule* start_script( const ScriptDef& script, 
                                 BObjectImp* param0,
                                 BObjectImp* param1,
                                 BObjectImp* param2,
                                 BObjectImp* param3 ) throw()
{
    BObject bobj0( param0 ); // just to delete if it doesn't go somewhere else
    BObject bobj1( param1 );
    BObject bobj2( param2?param2:UninitObject::create() );
    BObject bobj3( param3?param3:UninitObject::create() );

    ref_ptr<EScriptProgram> program = find_script2( script );
    if (program.get() == NULL)
	{
		cerr << "Error reading script " << script.name() << endl;
        // throw runtime_error( "Error starting script" );
        return NULL;
	}

    auto_ptr<UOExecutor> ex( create_script_executor() );
    if (program->haveProgram)
    {
        if (param3 != NULL)
            ex->pushArg( param3 );
        if (param2 != NULL)
            ex->pushArg( param2 );
        if (param1 != NULL)
            ex->pushArg( param1 );
        if (param0 != NULL)
            ex->pushArg( param0 );
    }
	//ex->addModule( new FileExecutorModule( *ex ) );	
	UOExecutorModule* uoemod = new UOExecutorModule( *ex );
    ex->addModule( uoemod );	

    if (!ex->setProgram( program.get() ))
    {
        return NULL;
        //throw runtime_error( "Error starting script." );
    }

    ex->setDebugLevel( Executor::NONE );


	runlist.push_back( ex.release() );

    return uoemod;
}

// EXACTLY the same as start_script, except uses find_script2
UOExecutorModule* start_script( ref_ptr<EScriptProgram> program, BObjectImp* param )
{
    BObject bobj( param?param:UninitObject::create() ); // just to delete if it doesn't go somewhere else

    UOExecutor* ex = create_script_executor();
    if (program->haveProgram && (param != NULL))
    {
        ex->pushArg( param );
    }
	//ex->addModule( new FileExecutorModule( *ex ) );	
	UOExecutorModule* uoemod = new UOExecutorModule( *ex );
    ex->addModule( uoemod );	

    if (!ex->setProgram( program.get() ))
        throw runtime_error( "Error starting script." );

    ex->setDebugLevel( Executor::NONE );

	runlist.push_back( ex );

    return uoemod;
}

void add_common_exmods( Executor& ex )
{
    ex.addModule( new BasicExecutorModule( ex ) );
    ex.addModule( new BasicIoExecutorModule( ex, std::cout ) );
    ex.addModule( new ClilocExecutorModule( ex ) );
	ex.addModule( new MathExecutorModule( ex ) );
    ex.addModule( new UtilExecutorModule( ex ) );
	//ex.addModule( new FileExecutorModule( ex ) );	
    ex.addModule( new ConfigFileExecutorModule( ex ) );
    ex.addModule( new UBoatExecutorModule( ex ) );
    ex.addModule( new DataFileExecutorModule( ex ) );
    ex.addModule( new PolSystemExecutorModule( ex ) );
	ex.addModule( new AttributeExecutorModule( ex ) );
	ex.addModule( new VitalExecutorModule( ex ) );
	ex.addModule( new StorageExecutorModule( ex ) );
	ex.addModule( new GuildExecutorModule( ex ) );
	ex.addModule( new UnicodeExecutorModule( ex ) );
	ex.addModule( CreateFileAccessExecutorModule( ex ) );
}

bool run_script_to_completion_worker( UOExecutor& ex, EScriptProgram* prog )
{
    add_common_exmods( ex );
	ex.addModule( new UOExecutorModule( ex ) );	

    ex.setProgram( prog );
	
    ex.setDebugLevel( Executor::NONE );

	scripts_thread_script = ex.scriptname();

	if(config.report_rtc_scripts)
		cout << "Script " << ex.scriptname() << " running..";

	while (ex.runnable())
    {
        cout << ".";
        for( int i = 0; (i < 1000) && ex.runnable(); i++ )
        {
            scripts_thread_scriptPC = ex.PC;
			ex.execInstr();
        }
    }
    cout << endl;
    return (ex.error_ == false);
}

bool run_script_to_completion( const char *filename, BObjectImp* parameter )
{
    passert_always( parameter );
    BObject bobj( parameter );
    ref_ptr<EScriptProgram> program = find_script( filename );
	if (program.get() == NULL)
	{
        cerr << "Error reading script " << filename << endl;
        return false;
	}

    UOExecutor ex;
    if (program->haveProgram)
    {
        ex.pushArg( parameter );
    }
    return run_script_to_completion_worker( ex, program.get()  );
}

bool run_script_to_completion( const char *filename )
{
    ref_ptr<EScriptProgram> program = find_script( filename );
	if (program.get() == NULL)
	{
        cerr << "Error reading script " << filename << endl;
        return false;
	}

    UOExecutor ex;

    return run_script_to_completion_worker( ex, program.get()  );
}


BObjectImp* run_executor_to_completion( UOExecutor& ex, const ScriptDef& script )
{
    ref_ptr<EScriptProgram> program = find_script2( script );
	if (program.get() == NULL)
	{
        cerr << "Error reading script " << script.name() << endl;
        return new BError( "Unable to read script" );
	}

    add_common_exmods( ex );
	ex.addModule( new UOExecutorModule( ex ) );	

    ex.setProgram( program.get() );
	
    ex.setDebugLevel( Executor::NONE );

	scripts_thread_script = ex.scriptname();

	int i = 0;
    bool reported = false;
	while (ex.runnable())
    {
		scripts_thread_scriptPC = ex.PC;
		ex.execInstr();
        if (++i == 1000)
        {
            if (reported)
            {
                cout << ".." << ex.PC;
            }
            else
            {
				if(config.report_rtc_scripts)
				{
					cout << "Script " << script.name() << " running.." << ex.PC;
					reported = true;
				}
            }
            i = 0;
        }
    }
    if (reported)
        cout << endl;
    if (ex.error_)
        return new BError( "Script exited with an error condition" );

    if (ex.ValueStack.empty())
        return new BLong( 1 );
    else
        return ex.ValueStack.top().get()->impptr()->copy();
}

BObjectImp* run_script_to_completion( const ScriptDef& script )
{
    UOExecutor ex;
    
    return run_executor_to_completion( ex, script );
}


BObjectImp* run_script_to_completion( const ScriptDef& script, 
                                       BObjectImp* param1 )
{
//??    BObject bobj1( param1 );

    UOExecutor ex;
    
    ex.pushArg( param1 );

    return run_executor_to_completion( ex, script );
}


BObjectImp* run_script_to_completion( const ScriptDef& script, 
                                       BObjectImp* param1,
                                       BObjectImp* param2 )
{
    //?? BObject bobj1( param1 ), bobj2( param2 );

    UOExecutor ex;
    
    ex.pushArg( param2 );
    ex.pushArg( param1 );

    return run_executor_to_completion( ex, script );
}

BObjectImp* run_script_to_completion( const ScriptDef& script, 
                                       BObjectImp* param1,
                                       BObjectImp* param2,
                                       BObjectImp* param3 )
{
//??    BObject bobj1( param1 ), bobj2( param2 ), bobj3( param3 );

    UOExecutor ex;
    
    ex.pushArg( param3 );
    ex.pushArg( param2 );
    ex.pushArg( param1 );
  
    return run_executor_to_completion( ex, script );
}

BObjectImp* run_script_to_completion( const ScriptDef& script, 
                                       BObjectImp* param1,
                                       BObjectImp* param2,
                                       BObjectImp* param3,
									   BObjectImp* param4 )
{
    //??BObject bobj1( param1 ), bobj2( param2 ), bobj3( param3 ), bobj4( param4 );

    UOExecutor ex;
    
	ex.pushArg( param4 );
    ex.pushArg( param3 );
    ex.pushArg( param2 );
    ex.pushArg( param1 );
  
    return run_executor_to_completion( ex, script );
}

BObjectImp* run_script_to_completion( const ScriptDef& script, 
                                       BObjectImp* param1,
                                       BObjectImp* param2,
                                       BObjectImp* param3,
									   BObjectImp* param4,
									   BObjectImp* param5)
{
   //?? BObject bobj1( param1 ), bobj2( param2 ), bobj3( param3 ),
	//??		bobj4( param4 ), bobj5( param5 );

    UOExecutor ex;

	ex.pushArg( param5 );
	ex.pushArg( param4 );
    ex.pushArg( param3 );
    ex.pushArg( param2 );
    ex.pushArg( param1 );

    return run_executor_to_completion( ex, script );
}

BObjectImp* run_script_to_completion( const ScriptDef& script, 
                                       BObjectImp* param1,
                                       BObjectImp* param2,
                                       BObjectImp* param3,
									   BObjectImp* param4,
									   BObjectImp* param5,
                                       BObjectImp* param6)
{
   //?? BObject bobj1( param1 ), bobj2( param2 ), bobj3( param3 ),
	//??		bobj4( param4 ), bobj5( param5 );

    UOExecutor ex;

    ex.pushArg( param6 );
	ex.pushArg( param5 );
	ex.pushArg( param4 );
    ex.pushArg( param3 );
    ex.pushArg( param2 );
    ex.pushArg( param1 );

    return run_executor_to_completion( ex, script );
}

BObjectImp* run_script_to_completion( const ScriptDef& script, 
                                       BObjectImp* param1,
                                       BObjectImp* param2,
                                       BObjectImp* param3,
									   BObjectImp* param4,
									   BObjectImp* param5,
                                       BObjectImp* param6,
                                       BObjectImp* param7)
{
    UOExecutor ex;

    ex.pushArg( param7 );
    ex.pushArg( param6 );
	ex.pushArg( param5 );
	ex.pushArg( param4 );
    ex.pushArg( param3 );
    ex.pushArg( param2 );
    ex.pushArg( param1 );

    return run_executor_to_completion( ex, script );
}

bool call_script( const ScriptDef& script, 
                  BObjectImp* param0 )
{
    try
    {
        BObject ob(run_script_to_completion(script,param0));
        return ob.isTrue();
    }
    catch(std::exception&)//...
    {
        return false;
    }
}
bool call_script( const ScriptDef& script, 
                  BObjectImp* param0,
                  BObjectImp* param1 )
{
    try
    {
        BObject ob(run_script_to_completion(script,param0,param1));
        return ob.isTrue();
    }
    catch(std::exception&)//...
    {
        return false;
    }
}
bool call_script( const ScriptDef& script, 
                  BObjectImp* param0,
                  BObjectImp* param1,
                  BObjectImp* param2 )
{
    try
    {
        BObject ob(run_script_to_completion(script,param0,param1,param2));
        return ob.isTrue();
    }
    catch(std::exception&)//...
    {
        return false;
    }
}
bool call_script( const ScriptDef& script, 
                  BObjectImp* param0,
                  BObjectImp* param1,
                  BObjectImp* param2,
				  BObjectImp* param3 )
{
    try
    {
        BObject ob(run_script_to_completion(script,param0,param1,param2,param3));
        return ob.isTrue();
    }
    catch(std::exception&)//...
    {
        return false;
    }
}
bool call_script( const ScriptDef& script, 
                  BObjectImp* param0,
                  BObjectImp* param1,
                  BObjectImp* param2,
				  BObjectImp* param3,
				  BObjectImp* param4)
{
    try
    {
        BObject ob(run_script_to_completion(script,param0,param1,param2,param3,param4));
        return ob.isTrue();
    }
    catch(std::exception&)//...
    {
        return false;
    }
}
bool call_script( const ScriptDef& script, 
                  BObjectImp* param0,
                  BObjectImp* param1,
                  BObjectImp* param2,
				  BObjectImp* param3,
				  BObjectImp* param4,
                  BObjectImp* param5)
{
    try
    {
        BObject ob(run_script_to_completion(script,param0,param1,param2,param3,param4,param5));
        return ob.isTrue();
    }
    catch(std::exception&)//...
    {
        return false;
    }
}
bool call_script( const ScriptDef& script, 
                  BObjectImp* param0,
                  BObjectImp* param1,
                  BObjectImp* param2,
				  BObjectImp* param3,
				  BObjectImp* param4,
                  BObjectImp* param5,
                  BObjectImp* param6)
{
    try
    {
        BObject ob(run_script_to_completion(script,param0,param1,param2,param3,param4,param5,param6));
        return ob.isTrue();
    }
    catch(std::exception&)//...
    {
        return false;
    }
}

UOExecutor *create_script_executor()
{
	UOExecutor *ex = new UOExecutor();

    add_common_exmods( *ex );
	//ex->addModule( new UOExecutorModule( *ex ) );	
	return ex;
}

UOExecutor *create_full_script_executor()
{
	UOExecutor *ex = new UOExecutor();

    add_common_exmods( *ex );
	ex->addModule( new UOExecutorModule( *ex ) );	
	return ex;
}

void schedule_executor( UOExecutor* ex )
{
    ex->setDebugLevel( Executor::NONE );
	// ex->initExec();

    if (ex->runnable())
    {
        runlist.push_back( ex );
    }
    else
    {
        delete ex;
    	/*
          deadlist.push_back( ex );
          script_done( ex );
        */
    }
}

void deschedule_executor( UOExecutor* ex )
{
	runlist.remove( ex );
    ranlist.remove( ex );
    if (ex->os_module->blocked_)
    {
        if (ex->os_module->in_hold_list_ == OSExecutorModule::TIMEOUT_LIST)
        {
            holdlist.erase( ex->os_module->hold_itr_ );
            ex->os_module->in_hold_list_ = OSExecutorModule::NO_LIST;
        }
        else if (ex->os_module->in_hold_list_ == OSExecutorModule::NOTIMEOUT_LIST)
        {
            notimeoutholdlist.erase( ex );
            ex->os_module->in_hold_list_ = OSExecutorModule::NO_LIST;
        }
    }
    if (ex->os_module->in_hold_list_ == OSExecutorModule::DEBUGGER_LIST)
    {
        debuggerholdlist.erase( ex );
        ex->os_module->in_hold_list_ = OSExecutorModule::NO_LIST;
    }
}

void list_script( UOExecutor* uoexec )
{
    cout << uoexec->prog_->name;
    if (uoexec->Globals2.size())
        cout << " Gl=" << uoexec->Globals2.size();
    if (uoexec->Locals2 && uoexec->Locals2->size())
        cout << " Lc=" << uoexec->Locals2->size();
    if (uoexec->ValueStack.size())
        cout << " VS=" << uoexec->ValueStack.size();
    if (uoexec->upperLocals2.size())
        cout << " UL=" << uoexec->upperLocals2.size();
    if (uoexec->ControlStack.size())
        cout << " CS=" << uoexec->ControlStack.size();
    cout << endl;
}

void list_scripts( const char* desc, ExecList& ls )
{
    cout << desc << " scripts:" << endl;
    ForEach( ls, list_script );
}

void list_scripts()
{
    list_scripts( "running", runlist );
    // list_scripts( "holding", holdlist );
    list_scripts( "ran", ranlist );
}

void list_crit_script( UOExecutor* uoexec )
{
    if (uoexec->os_module->critical)
        list_script( uoexec );
}
void list_crit_scripts( const char* desc, ExecList& ls )
{
    cout << desc << " scripts:" << endl;
    ForEach( ls, list_crit_script );
}

void list_crit_scripts()
{
    list_crit_scripts( "running", runlist );
    //list_crit_scripts( "holding", holdlist );
    list_crit_scripts( "ran", ranlist );
}

BObjectImp* OSExecutorModule::mf_OpenURL()
{
    Character* chr;
    const String* str;
    if (getCharacterParam( 0, chr ) &&
        ( (str = getStringParam( 1 )) != NULL ) )
    {
        if (chr->has_active_client())
        {
			MSGA5_OPEN_WEB_BROWSER msg;
		    unsigned urllen;
			const char *url = str->data();

		    urllen = strlen(url);
		    if (urllen > sizeof msg.address)
				urllen = sizeof msg.address;

			msg.msgtype = MSGA5_OPEN_WEB_BROWSER_ID;
			msg.msglen = ctBEu16( (urllen+3) );
			memcpy( msg.address, url, urllen );
			msg.null_term = 0;
			chr->client->transmit( &msg, (urllen+3) );
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
