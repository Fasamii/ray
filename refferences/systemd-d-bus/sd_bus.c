#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-daemon.h>
#include <unistd.h>

// Function to print D-Bus errors
void print_dbus_error(int result, sd_bus_error *error) {
	if (result != 0) {
		printf("Error code: %d\n", result);
		printf("System error: %s\n", strerror(-result));

		if (error->name) {
			printf("D-Bus error name: %s\n", error->name);
		}
		if (error->message) {
			printf("D-Bus error message: %s\n", error->message);
		}
	}
}

int demo_basic_dbus_call(void);
int demo_systemd_service_control(void);
int demo_property_queries(void);
int demo_signal_handling(void);
int create_simple_dbus_service(void);

int main(void) {

	demo_basic_dbus_call();
	printf("[-]\n");
	demo_systemd_service_control();
	printf("[-]\n");
	demo_property_queries();
	printf("[-]\n");
	demo_signal_handling();
	printf("[-]\n");
	create_simple_dbus_service();

	return 0;
}

int demo_basic_dbus_call(void) {
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message *reply = NULL;
	sd_bus *dbus = NULL;
	const char *hostname;
	int res;

	printf("[ ] connecting to system D-Bus...\n");

	res = sd_bus_open_system(&dbus);
	if (res < 0) {
		printf("[!] Failed to connect to system bus: %s\n", strerror(res));
		return res;
	}

	res = sd_bus_call_method(
	    dbus, "org.freedesktop.hostname1", "/org/freedesktop/hostname1",
	    "org.freedesktop.DBus.Properties", "Get", &error, &reply, "ss",
	    "org.freedesktop.hostname1", "Hostname");
	if (res < 0) {
		printf("[!] Failed to get hostname: %s\n", error.message);
		goto finish;
	}

	res = sd_bus_message_read(reply, "v", "s", &hostname);
	if (res < 0) {
		printf("[!] Failed to parse reply: %s\n", strerror(res));
		goto finish;
	}

	printf("[*] System Hostname : \e[38;5;5m%s\e[0m\n", hostname);

finish:
	printf("[ ] unref/free sd_bus program memory\n");
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	sd_bus_unref(dbus);
	return res;
}

int demo_systemd_service_control(void) {
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message *reply = NULL;
	sd_bus *bus = NULL;
	const char *unit_name =
	    "systemd-logind.service"; // Always running service
	int r;

	printf("[ ] Querying systemd unit: %s\n", unit_name);

	r = sd_bus_open_system(&bus);
	if (r < 0) {
		printf("[!] Failed to connect to system bus: %s\n", strerror(-r));
		return r;
	}

	// Get unit object path
	r = sd_bus_call_method(bus, "org.freedesktop.systemd1",
				     "/org/freedesktop/systemd1",
				     "org.freedesktop.systemd1.Manager", "GetUnit",
				     &error, &reply, "s", unit_name);

	if (r < 0) {
		printf("[!] Failed to get unit %s: %s\n", unit_name, error.message);
		goto fallback;
	}

	// Get the unit object path
	const char *unit_path;
	r = sd_bus_message_read(reply, "o", &unit_path);
	if (r < 0) {
		printf("[!] Failed to read unit path: %s\n", strerror(-r));
		goto fallback;
	}

	printf("[*] Found unit: \e[38;5;5m%s\e[0m\n", unit_name);
	printf("[*] Unit object path: \e[38;5;5m%s\e[0m\n", unit_path);

	printf("[ ] unref/free dbus resources");
	sd_bus_message_unref(reply);
	reply = NULL;
	sd_bus_error_free(&error);

	// Now query the unit's properties
	printf("\n[ ] Querying unit properties...\n");

	// Get ActiveState
	r = sd_bus_call_method(bus, "org.freedesktop.systemd1", unit_path,
				     "org.freedesktop.DBus.Properties", "Get", &error,
				     &reply, "ss", "org.freedesktop.systemd1.Unit",
				     "ActiveState");

	if (r >= 0) {
		const char *active_state;
		r = sd_bus_message_read(reply, "v", "s", &active_state);
		if (r >= 0) {
			printf("[*] Active State: \e[38;5;5m%s\e[0m\n", active_state);
		}
	}

	printf("[ ] unref/free dbus resources\n");
	sd_bus_message_unref(reply);
	reply = NULL;
	sd_bus_error_free(&error);

	// Get LoadState
	r = sd_bus_call_method(bus, "org.freedesktop.systemd1", unit_path,
				     "org.freedesktop.DBus.Properties", "Get", &error,
				     &reply, "ss", "org.freedesktop.systemd1.Unit",
				     "LoadState");

	if (r >= 0) {
		const char *load_state;
		r = sd_bus_message_read(reply, "v", "s", &load_state);
		if (r >= 0) {
			printf("[*] Load State: \e[38;5;5m%s\e[0m\n", load_state);
		}
	}

	sd_bus_message_unref(reply);
	reply = NULL;
	sd_bus_error_free(&error);

	// Get SubState (more detailed state)
	r = sd_bus_call_method(bus, "org.freedesktop.systemd1", unit_path,
				     "org.freedesktop.DBus.Properties", "Get", &error,
				     &reply, "ss", "org.freedesktop.systemd1.Unit",
				     "SubState");

	if (r >= 0) {
		const char *sub_state;
		r = sd_bus_message_read(reply, "v", "s", &sub_state);
		if (r >= 0) {
			printf("[*] Sub State: \e[38;5;5m%s\e[0m\n", sub_state);
		}
	}

	printf("[ ] unref/free dbus resources\n");
	sd_bus_message_unref(reply);
	reply = NULL;
	sd_bus_error_free(&error);

	// Get unit description
	r = sd_bus_call_method(bus, "org.freedesktop.systemd1", unit_path,
				     "org.freedesktop.DBus.Properties", "Get", &error,
				     &reply, "ss", "org.freedesktop.systemd1.Unit",
				     "Description");

	if (r >= 0) {
		const char *description;
		r = sd_bus_message_read(reply, "v", "s", &description);
		if (r >= 0) {
			printf("[*] Description: \e[38;5;5m%s\e[0m\n", description);
		}
	}

	goto skip;

fallback:
	printf("[?] Falling back to systemd version query...\n");
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	reply = NULL;

	// Get systemd version as fallback
	r = sd_bus_call_method(
	    bus, "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
	    "org.freedesktop.DBus.Properties", "Get", &error, &reply, "ss",
	    "org.freedesktop.systemd1.Manager", "Version");

	if (r >= 0) {
		const char *version;
		r = sd_bus_message_read(reply, "v", "s", &version);
		if (r >= 0) {
			printf("[*] Systemd version: \e[38;5;5m%s\e[0\n", version);
		}
	}

skip:
	printf("[ ] unref/free dbus resources\n");
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);
	sd_bus_unref(bus);
	return 0;
}

int demo_property_queries(void) {
    
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    sd_bus *bus = NULL;
    int r;

    r = sd_bus_open_system(&bus);
    if (r < 0) {
        printf("Failed to connect to system bus: %s\n", strerror(-r));
        return r;
    }

    printf("Querying system information via D-Bus...\n\n");

    // Query boot time
    r = sd_bus_call_method(bus,
                          "org.freedesktop.systemd1",
                          "/org/freedesktop/systemd1",
                          "org.freedesktop.DBus.Properties",
                          "Get",
                          &error,
                          &reply,
                          "ss",
                          "org.freedesktop.systemd1.Manager",
                          "KernelTimestamp");

    if (r >= 0) {
        uint64_t boot_time;
        r = sd_bus_message_read(reply, "v", "t", &boot_time);
        if (r >= 0) {
            printf("✓ Kernel timestamp: %lu microseconds\n", boot_time);
        }
    } else {
        printf("Could not get boot time: %s\n", error.message);
    }

    sd_bus_error_free(&error);
    sd_bus_message_unref(reply);
    reply = NULL;

    // Query system state
    r = sd_bus_call_method(bus,
                          "org.freedesktop.systemd1",
                          "/org/freedesktop/systemd1",
                          "org.freedesktop.DBus.Properties",
                          "Get",
                          &error,
                          &reply,
                          "ss",
                          "org.freedesktop.systemd1.Manager",
                          "SystemState");

    if (r >= 0) {
        const char *state;
        r = sd_bus_message_read(reply, "v", "s", &state);
        if (r >= 0) {
            printf("✓ System state: %s\n", state);
        }
    } else {
        printf("Could not get system state: %s\n", error.message);
    }

    printf("\nProperty Query Patterns:\n");
    printf("• Use org.freedesktop.DBus.Properties interface\n");
    printf("• Get method takes (interface_name, property_name)\n");
    printf("• Returns variant type - need to unwrap with sd_bus_message_read\n");
    printf("• Common patterns: 'v' + actual type signature\n");

    sd_bus_error_free(&error);
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);
    return 0;
}

int demo_signal_handling(void) {
    
    sd_bus *bus = NULL;
    int r;

    printf("Setting up signal monitoring...\n");
    
    r = sd_bus_open_system(&bus);
    if (r < 0) {
        printf("Failed to connect to system bus: %s\n", strerror(-r));
        return r;
    }

    printf("✓ Connected to system bus\n");
    
    // Add match for systemd unit state changes
    r = sd_bus_add_match(bus,
                        NULL,  // slot (we don't need to store it)
                        "type='signal',"
                        "sender='org.freedesktop.systemd1',"  
                        "interface='org.freedesktop.systemd1.Manager',"
                        "member='UnitNew'",
                        NULL,  // callback (NULL = just demonstrate setup)
                        NULL); // userdata

    if (r < 0) {
        printf("Failed to add signal match: %s\n", strerror(-r));
    } else {
        printf("✓ Signal match added for unit changes\n");
    }

    printf("\nSignal Monitoring Explanation:\n");
    printf("• sd_bus_add_match(): Subscribe to specific signals\n");
    printf("• Match rules filter signals by sender, interface, member\n");
    printf("• Callback function processes received signals\n");
    printf("• sd_bus_process(): Process pending messages\n");
    printf("• sd_bus_wait(): Wait for messages with timeout\n");
    
    printf("\nExample match rule components:\n");
    printf("• type='signal': Only match signals\n");
    printf("• sender='...': Filter by sending service\n");
    printf("• interface='...': Filter by D-Bus interface\n");
    printf("• member='...': Filter by signal name\n");
    printf("• path='...': Filter by object path\n");

    printf("\n(Skipping actual signal wait for demo purposes)\n");

    sd_bus_unref(bus);
    return 0;
}

 int create_simple_dbus_service(void) {
    printf("Here's how you'd structure a D-Bus service:\n\n");
    
    printf("1. SERVICE SETUP:\n");
    printf("   sd_bus *bus;\n");
    printf("   sd_bus_open_user(&bus);  // or sd_bus_open_system\n");
    printf("   sd_bus_request_name(bus, \"com.example.MyService\", 0);\n\n");
    
    printf("2. OBJECT REGISTRATION:\n");
    printf("   sd_bus_add_object_vtable(bus, &slot,\n");
    printf("                           \"/com/example/Object\",\n");
    printf("                           \"com.example.Interface\",\n");
    printf("                           vtable, userdata);\n\n");
    
    printf("3. VTABLE DEFINITION:\n");
    printf("   static const sd_bus_vtable vtable[] = {\n");
    printf("       SD_BUS_VTABLE_START(0),\n");  
    printf("       SD_BUS_METHOD(\"MethodName\", \"s\", \"s\", method_handler, 0),\n");
    printf("       SD_BUS_PROPERTY(\"PropName\", \"s\", prop_getter, 0, 0),\n");
    printf("       SD_BUS_SIGNAL(\"SignalName\", \"s\", 0),\n");
    printf("       SD_BUS_VTABLE_END\n");
    printf("   };\n\n");
    
    printf("4. EVENT LOOP:\n");
    printf("   for (;;) {\n");
    printf("       sd_bus_process(bus, NULL);\n");
    printf("       sd_bus_wait(bus, (uint64_t) -1);\n");
    printf("   }\n\n");
    
    printf("Key Concepts:\n");
    printf("• Vtable: Virtual method table defining interface\n");
    printf("• Methods: Functions other processes can call\n");
    printf("• Properties: Values other processes can get/set\n");
    printf("• Signals: Notifications sent to subscribers\n");
    printf("• Event loop: Process incoming D-Bus messages\n");
    
    return 0;
}
