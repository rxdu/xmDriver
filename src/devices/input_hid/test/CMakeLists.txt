# Note: the former test_js_event / test_kb_event / test_both_event demos used
# libevent directly (predating HidEventListener). They were removed with the
# libevent -> epoll change; the feature/ demos exercise the same via the
# HidEventListener abstraction.

add_subdirectory(feature)
