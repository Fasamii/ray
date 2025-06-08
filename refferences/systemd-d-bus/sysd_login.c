#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>  // For major() and minor() macros
#include <sys/ioctl.h>      // For ioctl() calls
#include <linux/input.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-login.h>

/*
 * SYSTEMD-LOGIND INPUT DEVICE ACCESS DEMO
 * 
 * This program demonstrates:
 * 1. Using systemd-logind to get session information
 * 2. Taking control of input devices via logind
 * 3. Opening and reading from input event files
 * 4. Releasing device control properly
 * 5. Handling permissions through systemd session management
 */

// Function prototypes
int demo_session_info(void);
int demo_device_control_and_access(void);
int find_keyboard_device(char *device_path, size_t path_size);
int read_input_events(int fd, int max_events);
const char* event_type_name(int type);
const char* key_name(int code);
void print_section(const char* title);

int main(int argc, char *argv[]) {
    printf("=== SYSTEMD-LOGIND INPUT DEVICE ACCESS DEMO ===\n\n");
    
    printf("This program demonstrates:\n");
    printf("• Getting session information via systemd-logind\n");
    printf("• Taking control of input devices\n");
    printf("• Opening /dev/input/eventX files\n");
    printf("• Reading input events (keyboard/mouse)\n");
    printf("• Proper device control management\n\n");
    
    // Check if we're running in a user session
    char *session_id = NULL;
    int r = sd_pid_get_session(getpid(), &session_id);
    if (r < 0) {
        printf("Warning: Not running in a systemd session: %s\n", strerror(-r));
        printf("This demo works best when run from a desktop session.\n\n");
    } else {
        printf("Running in systemd session: %s\n\n", session_id);
        free(session_id);
    }
    
    // Run demonstrations
    demo_session_info();
    demo_device_control_and_access();
    
    printf("\n=== Demo Complete ===\n");
    printf("To compile: gcc -o logind_demo program.c `pkg-config --cflags --libs libsystemd` -o logind_demo\n");
    printf("Run as regular user (not root) from a desktop session for best results.\n");
    return 0;
}

void print_section(const char* title) {
    printf("\n" "=" "=" "=" " %s " "=" "=" "=" "\n", title);
}

/*
 * DEMONSTRATION 1: Session Information
 * Shows how to get current session info via systemd-logind
 */
int demo_session_info(void) {
    print_section("Session Information via systemd-logind");
    
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    sd_bus *bus = NULL;
    char *session_id = NULL;
    int r;

    // Get our session ID
    r = sd_pid_get_session(getpid(), &session_id);
    if (r < 0) {
        printf("Failed to get session ID: %s\n", strerror(-r));
        printf("You might not be running in a systemd user session.\n");
        return r;
    }
    
    printf("✓ Current session ID: %s\n", session_id);
    
    // Connect to system bus to query logind
    r = sd_bus_open_system(&bus);
    if (r < 0) {
        printf("Failed to connect to system bus: %s\n", strerror(-r));
        free(session_id);
        return r;
    }
    
    // Get session object path
    char session_path[256];
    snprintf(session_path, sizeof(session_path), "/org/freedesktop/login1/session/%s", session_id);
    
    printf("Session object path: %s\n", session_path);
    
    // Query session properties
    printf("\nQuerying session properties...\n");
    
    // Get session type
    r = sd_bus_call_method(bus,
                          "org.freedesktop.login1",
                          session_path,
                          "org.freedesktop.DBus.Properties",
                          "Get",
                          &error,
                          &reply,
                          "ss",
                          "org.freedesktop.login1.Session",
                          "Type");
    
    if (r >= 0) {
        const char *session_type;
        r = sd_bus_message_read(reply, "v", "s", &session_type);
        if (r >= 0) {
            printf("✓ Session Type: %s\n", session_type);
        }
    } else {
        printf("Could not get session type: %s\n", error.message);
    }
    
    sd_bus_message_unref(reply);
    reply = NULL;
    sd_bus_error_free(&error);
    
    // Get session class
    r = sd_bus_call_method(bus,
                          "org.freedesktop.login1",
                          session_path,
                          "org.freedesktop.DBus.Properties",
                          "Get",
                          &error,
                          &reply,
                          "ss",
                          "org.freedesktop.login1.Session",
                          "Class");
    
    if (r >= 0) {
        const char *session_class;
        r = sd_bus_message_read(reply, "v", "s", &session_class);
        if (r >= 0) {
            printf("✓ Session Class: %s\n", session_class);
        }
    }
    
    sd_bus_message_unref(reply);
    reply = NULL;
    sd_bus_error_free(&error);
    
    // Get session state
    r = sd_bus_call_method(bus,
                          "org.freedesktop.login1",
                          session_path,
                          "org.freedesktop.DBus.Properties",
                          "Get",
                          &error,
                          &reply,
                          "ss",
                          "org.freedesktop.login1.Session",
                          "State");
    
    if (r >= 0) {
        const char *session_state;
        r = sd_bus_message_read(reply, "v", "s", &session_state);
        if (r >= 0) {
            printf("✓ Session State: %s\n", session_state);
        }
    }
    
    printf("\nSession Information Explanation:\n");
    printf("• sd_pid_get_session(): Get session ID for a process\n");
    printf("• Session Types: x11, wayland, tty, unspecified\n");
    printf("• Session Classes: user, greeter, lock-screen, background\n");
    printf("• Session States: online, active, closing\n");
    printf("• Active sessions can control devices\n");

    sd_bus_error_free(&error);
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);
    free(session_id);
    return 0;
}

/*
 * DEMONSTRATION 2: Device Control and Input Access
 * Shows how to take control of devices and read input events
 */
int demo_device_control_and_access(void) {
    print_section("Device Control and Input Event Reading");
    
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    sd_bus *bus = NULL;
    char *session_id = NULL;
    char device_path[256] = {0};
    int device_fd = -1;
    int r;

    // Get session ID
    r = sd_pid_get_session(getpid(), &session_id);
    if (r < 0) {
        printf("Failed to get session ID: %s\n", strerror(-r));
        return r;
    }
    
    // Find a keyboard device
    r = find_keyboard_device(device_path, sizeof(device_path));
    if (r < 0) {
        printf("Failed to find keyboard device\n");
        free(session_id);
        return r;
    }
    
    printf("Found input device: %s\n", device_path);
    
    // Connect to system bus
    r = sd_bus_open_system(&bus);
    if (r < 0) {
        printf("Failed to connect to system bus: %s\n", strerror(-r));
        free(session_id);
        return r;
    }
    
    // Get session object path
    char session_path[256];
    snprintf(session_path, sizeof(session_path), "/org/freedesktop/login1/session/%s", session_id);
    
    printf("Taking control of device via logind...\n");
    
    // Get device major:minor numbers
    struct stat st;
    if (stat(device_path, &st) < 0) {
        printf("Failed to stat device: %s\n", strerror(errno));
        goto cleanup;
    }
    
    unsigned int major_num = major(st.st_rdev);
    unsigned int minor_num = minor(st.st_rdev);
    
    printf("Device major:minor = %u:%u\n", major_num, minor_num);
    
    // Take control of the device through logind
    r = sd_bus_call_method(bus,
                          "org.freedesktop.login1",
                          session_path,
                          "org.freedesktop.login1.Session",
                          "TakeDevice",
                          &error,
                          &reply,
                          "uu",
                          major_num, minor_num);
    
    if (r < 0) {
        printf("Failed to take device control: %s\n", error.message);
        printf("This might be because:\n");
        printf("• Not running in an active graphical session\n");
        printf("• Device already controlled by another process\n");
        printf("• Insufficient permissions\n");
        goto try_direct_access;
    }
    
    // Get the file descriptor from the reply
    int controlled_fd;
    int paused;
    r = sd_bus_message_read(reply, "hb", &controlled_fd, &paused);
    if (r < 0) {
        printf("Failed to read TakeDevice reply: %s\n", strerror(-r));
        goto cleanup;
    }
    
    printf("✓ Successfully took control of device\n");
    printf("✓ Got file descriptor: %d (paused: %s)\n", controlled_fd, paused ? "yes" : "no");
    
    if (!paused) {
        printf("\nReading input events (press some keys, Ctrl+C to stop)...\n");
        read_input_events(controlled_fd, 10);
    }
    
    // Release device control
    sd_bus_message_unref(reply);
    reply = NULL;
    sd_bus_error_free(&error);
    
    printf("\nReleasing device control...\n");
    r = sd_bus_call_method(bus,
                          "org.freedesktop.login1",
                          session_path,
                          "org.freedesktop.login1.Session",
                          "ReleaseDevice",
                          &error,
                          &reply,
                          "uu",
                          major_num, minor_num);
    
    if (r >= 0) {
        printf("✓ Released device control\n");
    } else {
        printf("Failed to release device: %s\n", error.message);
    }
    
    close(controlled_fd);
    goto explanation;

try_direct_access:
    printf("\nTrying direct device access (may fail due to permissions)...\n");
    device_fd = open(device_path, O_RDONLY | O_NONBLOCK);
    if (device_fd < 0) {
        printf("Failed to open device directly: %s\n", strerror(errno));
        printf("This is expected - input devices require special permissions.\n");
    } else {
        printf("✓ Opened device directly (unusual - check permissions)\n");
        printf("Reading a few events...\n");
        read_input_events(device_fd, 5);
        close(device_fd);
    }

explanation:
    printf("\nDevice Control via systemd-logind:\n");
    printf("• TakeDevice: Gain exclusive access to a device\n");
    printf("• Returns file descriptor for device access\n"); 
    printf("• ReleaseDevice: Release control when done\n");
    printf("• Only works for active sessions\n");
    printf("• Handles device permissions automatically\n");
    
    printf("\nWhy use logind for device access:\n");
    printf("• Automatic permission management\n");
    printf("• Session-aware device control\n");
    printf("• Prevents conflicts between sessions\n");
    printf("• Integrates with seat management\n");
    printf("• Handles device hotplug events\n");

cleanup:
    sd_bus_error_free(&error);
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);
    free(session_id);
    return 0;
}

/*
 * Helper function to find a keyboard device
 */
int find_keyboard_device(char *device_path, size_t path_size) {
    // Try common keyboard device paths
    const char *candidates[] = {
        "/dev/input/event0",
        "/dev/input/event1", 
        "/dev/input/event2",
        "/dev/input/event3",
        "/dev/input/event4",
        NULL
    };
    
    for (int i = 0; candidates[i]; i++) {
        int fd = open(candidates[i], O_RDONLY);
        if (fd >= 0) {
            // Check if it supports key events
            unsigned long evbit = 0;
            if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), &evbit) >= 0) {
                if (evbit & (1 << EV_KEY)) {
                    close(fd);
                    strncpy(device_path, candidates[i], path_size - 1);
                    device_path[path_size - 1] = '\0';
                    return 0;
                }
            }
            close(fd);
        }
    }
    
    return -1;
}

/*
 * Read and display input events
 */
int read_input_events(int fd, int max_events) {
    struct input_event ev;
    int count = 0;
    
    printf("Event format: [type:code:value] description\n");
    printf("Press keys or move mouse (reading %d events max):\n\n", max_events);
    
    while (count < max_events) {
        ssize_t bytes = read(fd, &ev, sizeof(ev));
        if (bytes == sizeof(ev)) {
            if (ev.type != EV_SYN) {  // Skip sync events for cleaner output
                printf("[%d:%d:%d] %s", 
                       ev.type, ev.code, ev.value,
                       event_type_name(ev.type));
                
                if (ev.type == EV_KEY) {
                    printf(" %s %s", 
                           key_name(ev.code),
                           ev.value ? (ev.value == 1 ? "PRESS" : "REPEAT") : "RELEASE");
                }
                printf("\n");
                count++;
            }
        } else if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(10000);  // 10ms delay
                continue;
            } else {
                printf("Read error: %s\n", strerror(errno));
                break;
            }
        }
    }
    
    return count;
}

/*
 * Helper functions for event interpretation
 */
const char* event_type_name(int type) {
    switch (type) {
        case EV_SYN: return "SYN";
        case EV_KEY: return "KEY";
        case EV_REL: return "REL";
        case EV_ABS: return "ABS";
        case EV_MSC: return "MSC";
        case EV_LED: return "LED";
        case EV_SND: return "SND";
        case EV_REP: return "REP";
        default: return "UNKNOWN";
    }
}

const char* key_name(int code) {
    // Just a few common keys for demo
    switch (code) {
        case KEY_A: return "KEY_A";
        case KEY_B: return "KEY_B";
        case KEY_C: return "KEY_C";
        case KEY_SPACE: return "KEY_SPACE";
        case KEY_ENTER: return "KEY_ENTER";
        case KEY_ESC: return "KEY_ESC";
        case KEY_LEFTSHIFT: return "KEY_LEFTSHIFT";
        case KEY_RIGHTSHIFT: return "KEY_RIGHTSHIFT";
        case BTN_LEFT: return "BTN_LEFT";
        case BTN_RIGHT: return "BTN_RIGHT";
        case BTN_MIDDLE: return "BTN_MIDDLE";
        default: {
            static char buf[32];
            snprintf(buf, sizeof(buf), "KEY_%d", code);
            return buf;
        }
    }
}

/*
 * COMPILATION AND USAGE:
 * 
 * gcc -o logind_demo program.c `pkg-config --cflags --libs libsystemd`
 * 
 * IMPORTANT NOTES:
 * 
 * 1. RUN AS REGULAR USER (not root)
 * 2. Must be run from a graphical session (X11/Wayland)
 * 3. Session must be "active" for device control to work
 * 4. Input devices are typically /dev/input/eventX
 * 
 * SYSTEMD-LOGIND DEVICE CONTROL:
 * 
 * • TakeDevice(major, minor) -> (fd, paused)
 * • ReleaseDevice(major, minor)
 * • PauseDeviceComplete(major, minor)
 * • Signals: PauseDevice, ResumeDevice
 * 
 * PERMISSIONS:
 * Without logind: Input devices owned by 'input' group
 * With logind: Automatic permission management per session
 * 
 * USE CASES:
 * • Game controllers
 * • Custom input handling
 * • Kiosk applications  
 * • Accessibility tools
 * • Input remapping
 */
