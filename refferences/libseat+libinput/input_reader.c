#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <libudev.h>
#include <libinput.h>
#include <libseat.h>

struct input_context {
    struct libseat *seat;
    struct libinput *libinput;
    struct udev *udev;
    int running;
};

// Forward declarations
static int open_restricted(const char *path, int flags, void *user_data);
static void close_restricted(int fd, void *user_data);
static void handle_enable_seat(struct libseat *seat, void *data);
static void handle_disable_seat(struct libseat *seat, void *data);
static void print_event(struct libinput_event *event);

// libinput interface for opening/closing devices
static const struct libinput_interface interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

// libseat listener for session management
static const struct libseat_seat_listener seat_listener = {
    .enable_seat = handle_enable_seat,
    .disable_seat = handle_disable_seat,
};

// Open device through libseat (called by libinput)
static int open_restricted(const char *path, int flags, void *user_data) {
    (void)flags; // flags parameter required by libinput interface but not used
    struct input_context *ctx = user_data;
    int device_id;
    
    printf("Opening device: %s\n", path);
    
    int fd = libseat_open_device(ctx->seat, path, &device_id);
    if (fd < 0) {
        fprintf(stderr, "Failed to open device %s: %s\n", path, strerror(-fd));
        return -1;
    }
    
    return fd;
}

// Close device through libseat (called by libinput)
static void close_restricted(int fd, void *user_data) {
    struct input_context *ctx = user_data;
    printf("Closing device fd: %d\n", fd);
    libseat_close_device(ctx->seat, fd);
}

// Called when seat is enabled (session becomes active)
static void handle_enable_seat(struct libseat *seat, void *data) {
    (void)seat; // Required by libseat interface
    struct input_context *ctx = data;
    printf("Seat enabled - session is now active\n");
    ctx->running = 1;
}

// Called when seat is disabled (session becomes inactive)
static void handle_disable_seat(struct libseat *seat, void *data) {
    (void)seat; // Required by libseat interface
    struct input_context *ctx = data;
    printf("Seat disabled - session is now inactive\n");
    ctx->running = 0;
}

// Print details about input events
static void print_event(struct libinput_event *event) {
    enum libinput_event_type type = libinput_event_get_type(event);
    struct libinput_device *device = libinput_event_get_device(event);
    const char *device_name = libinput_device_get_name(device);
    
    switch (type) {
        case LIBINPUT_EVENT_DEVICE_ADDED:
            printf("Device added: %s\n", device_name);
            break;
            
        case LIBINPUT_EVENT_DEVICE_REMOVED:
            printf("Device removed: %s\n", device_name);
            break;
            
        case LIBINPUT_EVENT_KEYBOARD_KEY: {
            struct libinput_event_keyboard *kb_event = 
                libinput_event_get_keyboard_event(event);
            uint32_t key = libinput_event_keyboard_get_key(kb_event);
            enum libinput_key_state state = libinput_event_keyboard_get_key_state(kb_event);
            printf("Keyboard [%s]: Key %u %s\n", 
                   device_name, key, 
                   state == LIBINPUT_KEY_STATE_PRESSED ? "pressed" : "released");
            break;
        }
        
        case LIBINPUT_EVENT_POINTER_MOTION: {
            struct libinput_event_pointer *ptr_event = 
                libinput_event_get_pointer_event(event);
            double dx = libinput_event_pointer_get_dx(ptr_event);
            double dy = libinput_event_pointer_get_dy(ptr_event);
            printf("Mouse [%s]: Motion dx=%.2f dy=%.2f\n", device_name, dx, dy);
            break;
        }
        
        case LIBINPUT_EVENT_POINTER_BUTTON: {
            struct libinput_event_pointer *ptr_event = 
                libinput_event_get_pointer_event(event);
            uint32_t button = libinput_event_pointer_get_button(ptr_event);
            enum libinput_button_state state = libinput_event_pointer_get_button_state(ptr_event);
            printf("Mouse [%s]: Button %u %s\n", 
                   device_name, button,
                   state == LIBINPUT_BUTTON_STATE_PRESSED ? "pressed" : "released");
            break;
        }
        
        case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL: {
            struct libinput_event_pointer *ptr_event = 
                libinput_event_get_pointer_event(event);
            double v_scroll = libinput_event_pointer_get_scroll_value(ptr_event, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
            double h_scroll = libinput_event_pointer_get_scroll_value(ptr_event, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);
            printf("Mouse [%s]: Scroll vertical=%.2f horizontal=%.2f\n", device_name, v_scroll, h_scroll);
            break;
        }
        
        case LIBINPUT_EVENT_TOUCH_DOWN: {
            struct libinput_event_touch *touch_event = 
                libinput_event_get_touch_event(event);
            int32_t slot = libinput_event_touch_get_slot(touch_event);
            double x = libinput_event_touch_get_x_transformed(touch_event, 1920);
            double y = libinput_event_touch_get_y_transformed(touch_event, 1080);
            printf("Touch [%s]: Down slot=%d x=%.2f y=%.2f\n", device_name, slot, x, y);
            break;
        }
        
        case LIBINPUT_EVENT_TOUCH_UP: {
            struct libinput_event_touch *touch_event = 
                libinput_event_get_touch_event(event);
            int32_t slot = libinput_event_touch_get_slot(touch_event);
            printf("Touch [%s]: Up slot=%d\n", device_name, slot);
            break;
        }
        
        case LIBINPUT_EVENT_TOUCH_MOTION: {
            struct libinput_event_touch *touch_event = 
                libinput_event_get_touch_event(event);
            int32_t slot = libinput_event_touch_get_slot(touch_event);
            double x = libinput_event_touch_get_x_transformed(touch_event, 1920);
            double y = libinput_event_touch_get_y_transformed(touch_event, 1080);
            printf("Touch [%s]: Motion slot=%d x=%.2f y=%.2f\n", device_name, slot, x, y);
            break;
        }
        
        default:
            printf("Other event [%s]: type=%d\n", device_name, type);
            break;
    }
}

// Signal handler for graceful shutdown
static volatile int should_exit = 0;

static void signal_handler(int sig) {
    (void)sig;
    should_exit = 1;
    printf("\nReceived signal, shutting down...\n");
}

int main(int argc, char *argv[]) {
    (void)argc; // Command line arguments not used in this example
    (void)argv;
    
    struct input_context ctx = {0};
    int ret = EXIT_FAILURE;
    
    // Set up signal handler for Ctrl+C
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("Starting input event reader...\n");
    printf("Press Ctrl+C to exit\n\n");
    
    // Initialize udev
    printf("Initializing udev...\n");
    ctx.udev = udev_new();
    if (!ctx.udev) {
        fprintf(stderr, "Failed to initialize udev\n");
        goto cleanup;
    }
    printf("✓ udev initialized\n");
    
    // Open seat
    printf("Opening seat...\n");
    ctx.seat = libseat_open_seat(&seat_listener, &ctx);
    if (!ctx.seat) {
        fprintf(stderr, "Failed to open seat\n");
        fprintf(stderr, "Make sure you're running from a TTY and seatd/logind is running\n");
        fprintf(stderr, "Check if seatd service is running: systemctl status seatd\n");
        fprintf(stderr, "Or check logind: systemctl status systemd-logind\n");
        goto cleanup;
    }
    printf("✓ seat opened\n");
    
    // Initialize libinput
    printf("Initializing libinput...\n");
    ctx.libinput = libinput_udev_create_context(&interface, &ctx, ctx.udev);
    if (!ctx.libinput) {
        fprintf(stderr, "Failed to create libinput context\n");
        goto cleanup;
    }
    printf("✓ libinput context created\n");
    
    // Assign seat to libinput
    printf("Assigning seat to libinput...\n");
    if (libinput_udev_assign_seat(ctx.libinput, "seat0") != 0) {
        fprintf(stderr, "Failed to assign seat to libinput\n");
        goto cleanup;
    }
    printf("✓ seat assigned to libinput\n");
    
    printf("Successfully initialized input system\n");
    printf("Waiting for input events...\n\n");
    
    // Process initial device detection
    libinput_dispatch(ctx.libinput);
    struct libinput_event *event;
    while ((event = libinput_get_event(ctx.libinput))) {
        print_event(event);
        libinput_event_destroy(event);
    }
    
    // Main event loop
    ctx.running = 1;
    while (ctx.running && !should_exit) {
        struct pollfd fds[2];
        int nfds = 0;
        
        // Poll libinput fd
        fds[nfds].fd = libinput_get_fd(ctx.libinput);
        fds[nfds].events = POLLIN;
        nfds++;
        
        // Poll libseat fd
        int seat_fd = libseat_get_fd(ctx.seat);
        if (seat_fd >= 0) {
            fds[nfds].fd = seat_fd;
            fds[nfds].events = POLLIN;
            nfds++;
        }
        
        int poll_ret = poll(fds, nfds, 1000); // 1 second timeout
        
        if (poll_ret < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "Poll error: %s\n", strerror(errno));
            break;
        }
        
        if (poll_ret == 0) {
            // Timeout - just continue to check should_exit
            continue;
        }
        
        // Handle seat events
        if (nfds > 1 && fds[1].revents & POLLIN) {
            if (libseat_dispatch(ctx.seat, 0) < 0) {
                fprintf(stderr, "Failed to dispatch seat events\n");
                break;
            }
        }
        
        // Handle input events
        if (fds[0].revents & POLLIN) {
            if (libinput_dispatch(ctx.libinput) < 0) {
                fprintf(stderr, "Failed to dispatch input events\n");
                break;
            }
            
            while ((event = libinput_get_event(ctx.libinput))) {
                print_event(event);
                libinput_event_destroy(event);
            }
        }
    }
    
    ret = EXIT_SUCCESS;
    printf("\nShutting down...\n");
    
cleanup:
    if (ctx.libinput) {
        libinput_unref(ctx.libinput);
    }
    if (ctx.seat) {
        libseat_close_seat(ctx.seat);
    }
    if (ctx.udev) {
        udev_unref(ctx.udev);
    }
    
    return ret;
}
