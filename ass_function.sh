# ASS - Android Switch Serial
# Add this to your ~/.bashrc or ~/.zshrc:
#   source /path/to/MemoryTrafficProfiler/ass_function.sh

ass() {
    local devices=($(adb devices -l | grep -E "device\s" | awk '{print $1}' | sort))
    local current="${ANDROID_SERIAL:-}"
    local next_device=""
    
    if [ ${#devices[@]} -eq 0 ]; then
        echo "ERROR: No Android devices found" >&2
        return 1
    fi
    
    # If no current device or current device not in list, use first device
    if [ -z "$current" ] || ! echo "${devices[@]}" | grep -q "$current"; then
        next_device="${devices[0]}"
    else
        # Find current device index
        local current_idx=-1
        for i in "${!devices[@]}"; do
            if [ "${devices[$i]}" == "$current" ]; then
                current_idx=$i
                break
            fi
        done
        
        # Switch to next device (circular)
        local next_idx=$(( (current_idx + 1) % ${#devices[@]} ))
        next_device="${devices[$next_idx]}"
    fi
    
    export ANDROID_SERIAL="$next_device"
    echo "Switched to: $ANDROID_SERIAL"
}

ass_status() {
    local devices=($(adb devices -l | grep -E "device\s" | awk '{print $1}' | sort))
    local current="${ANDROID_SERIAL:-}"
    
    echo "Available devices:"
    for i in "${!devices[@]}"; do
        local marker=""
        if [ "${devices[$i]}" == "$current" ]; then
            marker=" ← current"
        fi
        echo "  [$i] ${devices[$i]}$marker"
    done
    
    if [ -z "$current" ]; then
        echo ""
        echo "Current ANDROID_SERIAL: (not set)"
    else
        echo ""
        echo "Current ANDROID_SERIAL: $current"
    fi
}
