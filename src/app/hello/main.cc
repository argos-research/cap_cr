#include <base/component.h>
#include <base/log.h>
#include <base/env.h>
#include <base/sleep.h>
#include <base/child.h>
#include <ram_session/connection.h>
#include <rom_session/connection.h>
#include <cpu_session/connection.h>
#include <cap_session/connection.h>
#include <pd_session/connection.h>
#include <loader_session/connection.h>
#include <region_map/client.h>
#include <timer_session/connection.h>

namespace Fiasco {
#include <l4/sys/kdebug.h>
}

class Hello_child_policy : public Genode::Child_policy
{
private:

	Genode::Parent_service    _log_service;
	Genode::Parent_service    _rm_service;

public:

	Hello_child_policy()
	: _log_service("LOG"), _rm_service("RM")
	{}

	/****************************
	 ** Child-policy interface **
	 ****************************/

	const char *name() const { return "hello_child"; }

	Genode::Service *resolve_session_request(const char *service, const char *)
	{
		/* forward white-listed session requests to our parent */
					return !Genode::strcmp(service, "LOG") ? &_log_service
					     : !Genode::strcmp(service, "RM")  ? &_rm_service
					     : 0;
	}

	void filter_session_args(const char *service, char *args, Genode::size_t args_len)
	{
		/* define session label for sessions forwarded to our parent */
		Genode::Arg_string::set_arg_string(args, args_len, "label", "hello_child");
	}
};

Genode::size_t Component::stack_size() { return 64*1024; }

void Component::construct(Genode::Env &env)
{
	using namespace Genode;

	Timer::Connection timer { env };

	const char* elf_name = "hello_child";

	raw("cap_cr|STAGE|start|");

	log("-------------------- Creating hello_child --------------------");
//	enter_kdebug("Before pd_connection");
	log("-------------------- Creating pd_connection --------------------");
	raw("cap_cr|STAGE|Pd_connection|");
	Genode::Pd_connection  pd(env, elf_name);
	log("-------------------- Pd_connection created --------------------");
//	enter_kdebug("After pd_connection");

	log("-------------------- Creating cpu_connection --------------------");
	raw("cap_cr|STAGE|Cpu_connection|");
	Genode::Cpu_connection cpu;
	log("-------------------- Cpu_connection created --------------------");

	// transfer some of our own ram quota to the new child
	enum { CHILD_QUOTA = 1*1024*1024 };
	log("-------------------- Creating ram_connection --------------------");
	raw("cap_cr|STAGE|Ram_connection|");
	Genode::Ram_connection ram;
	log("-------------------- Ram_connection created --------------------");
/*	ram.ref_account(Genode::env()->ram_session_cap());
	Genode::env()->ram_session()->transfer_quota(ram.cap(), CHILD_QUOTA);
*/
	log("-------------------- Referencing RAM account --------------------");
	raw("cap_cr|STAGE|ref_account|");
	ram.ref_account(env.ram_session_cap());
	log("-------------------- RAM account referenced --------------------");
	log("-------------------- Transfering RAM quota --------------------");
	raw("cap_cr|STAGE|transfer_quota|");
	env.ram().transfer_quota(ram.cap(), CHILD_QUOTA);
	log("-------------------- Ram quota transferred --------------------");

	log("-------------------- Creating Initial_thread --------------------");
	raw("cap_cr|STAGE|Initial_thread|");
	Genode::Child::Initial_thread _initial_thread(cpu, pd, "hello_child thread");
	log("-------------------- Initial_thread created --------------------");

	log("-------------------- Creating rom_connection --------------------");
	raw("cap_cr|STAGE|Rom_connection|");
	Genode::Rom_connection    _elf(elf_name);
	log("-------------------- Rom_connection created --------------------");

	log("-------------------- Creating Region_map_client --------------------");
	raw("cap_cr|STAGE|Region_map_client|");
	Genode::Region_map_client _address_space( pd.address_space() );
	log("-------------------- Region_map_client created --------------------");

	enum { STACK_SIZE = 8*1024 };
	log("-------------------- Creating Cap_connection --------------------");
	raw("cap_cr|STAGE|Cap_connection|");
	Cap_connection cap;
	log("-------------------- Cap_connection created --------------------");

	log("-------------------- Creating Rpc_entrypoint --------------------");
	raw("cap_cr|STAGE|Rpc_entrypoint|");
	Rpc_entrypoint ep(&cap, STACK_SIZE, "hello_child ep");
	log("-------------------- Rpc_entrypoint created --------------------");

	log("-------------------- Creating Hello_child_policy --------------------");
	raw("cap_cr|STAGE|Child_policy|");
	Hello_child_policy my_child_policy;
	log("-------------------- Hello_child_policy created --------------------");

	log("-------------------- Creating Child my_child --------------------");
	raw("cap_cr|STAGE|Child|");
//	enter_kdebug("Before Child");
	Child my_child(
					_elf.dataspace(), // valid dataspace that contains elf. loads and executes it.
				//	Genode::Dataspace_capability(), // invalid dataspace creates Child shell without loaded elf
					Genode::Dataspace_capability(), // normal second argument. not related to above.
					pd, pd,
					ram, ram,
					cpu, _initial_thread,
					*Genode::env()->rm_session(), _address_space,
					ep, my_child_policy);
	log("-------------------- Child my_child created --------------------");

	log("-------------------- Waiting 3 seconds --------------------");
//	raw("cap_cr|STAGE|sleep_start|");
	timer.msleep(3000);
	raw("cap_cr|STAGE|sleep_end");
	log("-------------------- Done! --------------------");

	Genode::log("Hello cap_cr!");
}

