#include <stdio.h>
#include <systemd/sd-bus.h>

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

int main(void) {
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message *dbusMessage = NULL;
	sd_bus *dbus = NULL;
	int result;

	// connecting to the system dbus //
	// foo takes address of variable storing bus //
	result = sd_bus_open_system(&dbus);
	if (result != 0) {
		print_dbus_error(result, &error);
		return 1;
	}

	result =
	    sd_bus_call_method(dbus,
				     "org.freedesktop.login1",    // service to contact
				     "/org/freedesktop/login1",   // object path
				     "org.freedesktop.DBus.Peer", // interface name
				     "GetMachineId",		    // method name
				     &error,	 // object to return error in
				     &dbusMessage, // return message on success
				     "");		 // input signature
	if (result < 0) {
		print_dbus_error(result, &error);
		return 2;
	}

	char *machineId = NULL;
	result = sd_bus_message_read(dbusMessage, "s", machineId);
	if (result < 0) {
		print_dbus_error(result, &error);
		return 3;
	}

	printf("MSG : %s\n", machineId);

	return 0;
}
