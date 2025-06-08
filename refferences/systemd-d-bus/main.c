#include <stdio.h>
#include <systemd/sd-bus.h>

int main(void) {
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message *dbusMessage = NULL;
	sd_bus *dbus = NULL;
	const char *path;
	int result;

	// connecting to the system dbus //
	// foo takes address of variable storing bus //
	result = sd_bus_open_system(&dbus);
	if (result != 0) {
		printf("Failed to connect to system dbus\n");
		return 1;
	}	

	// issuing the method call and store the response in dubsMessage variable //
	result = sd_bus_call_method(dbus,
		"org.freedesktop.systemd1",		// service to contact
            "/org/freedesktop/systemd1",			// object path
		"org.freedesktop.systemd1.Manager",	// interface name
            "StartUnit",					// method name
            &error,					// object to return error in
            &dbusMessage,					// return message on success
            "ss",						// input signature
            "cups.service",						// first argument
            "replace");							// second argument
	if (result != 0) {
		printf("Failed to perform a call method\n");
		return 2;
	}




	return 0;
}
