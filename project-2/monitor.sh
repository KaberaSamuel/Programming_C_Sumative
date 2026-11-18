#!/usr/bin/env bash

export LC_ALL=C

activity_log="${HOME}/.system-health.log"

log_activity() {
    printf '[%s] %s\n' "$(date '+%F %T')" "$*" >> "$activity_log" ||
        printf 'Could not write to the activity log.\n' >&2
}

check_tools() {
    local tool
    for tool in "$@"; do
        if ! command -v "$tool" >/dev/null; then
            printf 'Missing command: %s\n' "$tool" >&2
            return 1
        fi
    done
}

system_info() {
    check_tools hostname uname uptime date || return 1
    local os=Linux
    if [[ -r /etc/os-release ]]; then
        . /etc/os-release
        os=${PRETTY_NAME:-Linux}
    fi
    printf 'Hostname: %s\nOS: %s\nKernel: %s\nUptime: %s\nDate: %s\n' \
        "$(hostname)" "$os" "$(uname -r)" "$(uptime -p)" "$(date)"
}

read_threshold() {
    local value
    read -r -p "$1 threshold (0 to 100): " value || return 1
    if [[ ! $value =~ ^[0-9]{1,3}$ ]] || (( 10#$value > 100 )); then
        printf 'Enter a whole number from 0 to 100.\n' >&2
        return 1
    fi
    printf '%d\n' "$((10#$value))"
}

cpu_ticks() {
    awk '/^cpu / {
        total = 0
        for (i = 2; i <= 9; i++) total += $i
        printf "%.0f %.0f\n", total, $5 + $6
        exit
    }' /proc/stat
}

cpu_info() {
    check_tools awk sleep ps || return 1
    local first second total1 idle1 total2 idle2 elapsed usage processes
    first=$(cpu_ticks) && [[ -n $first ]] || return 1
    sleep 1 || return 1
    second=$(cpu_ticks) && [[ -n $second ]] || return 1
    read -r total1 idle1 <<< "$first"
    read -r total2 idle2 <<< "$second"
    elapsed=$((total2 - total1))
    if (( elapsed <= 0 || idle2 < idle1 || idle2 - idle1 > elapsed )); then
        printf 'Could not measure CPU usage. Try again.\n' >&2
        return 1
    fi
    usage=$((100 * (elapsed - idle2 + idle1) / elapsed))
    processes=$(ps -eo pid,comm,pcpu,pmem --sort=-pcpu) || return 1
    printf 'CPU usage: %d%%\n' "$usage"
    printf '\nTop processes by average CPU use\n'
    awk 'NR <= 6' <<< "$processes"
    if (( usage > ${1:-100} )); then
        printf 'Warning: CPU usage is above %s%%.\n' "$1"
    fi
}

memory_info() {
    check_tools free awk || return 1
    local memory
    memory=$(free -m) || return 1
    awk -v threshold="${1:-100}" '
        /^Mem:/ && $2 > 0 {
            usage = 100 * $3 / $2
            printf "Memory (MiB): %d total, %d used, %d available\n", $2, $3, $7
            printf "Memory usage: %.1f%%\n", usage
            if (usage > threshold)
                printf "Warning: memory usage is above %d%%.\n", threshold
            found = 1
        }
        END { if (!found) exit 1 }
    ' <<< "$memory"
}

disk_info() {
    check_tools df awk || return 1
    local disks
    disks=$(df -hP) || return 1
    printf '%s\n' "$disks"
    awk -v threshold="${1:-100}" '
        NR > 1 && $5 ~ /^[0-9]+%$/ && $5 + 0 > threshold {
            printf "Warning: %s is at %s, above %d%%.\n", $1, $5, threshold
        }
    ' <<< "$disks"
}

monitor() {
    local threshold result
    threshold=$(read_threshold "$1") || return
    if result=$("$2" "$threshold"); then
        printf '%s\n' "$result"
        log_activity "$1 monitoring (threshold ${threshold}%):"$'\n'"$result"
    else
        printf 'Could not read %s information.\n' "$1"
        log_activity "$1 monitoring failed."
    fi
}

show_menu() {
    printf '\nSystem Health Manager\n'
    printf '%s\n' '1. System information' '2. Monitor CPU' '3. Monitor memory' \
        '4. Monitor disks' '0. Exit'
}

main() {
    local choice
    while true; do
        show_menu
        read -r -p 'Choose an option: ' choice || break
        case "$choice" in
            1) system_info && log_activity 'Viewed system information.' ;;
            2) monitor CPU cpu_info ;;
            3) monitor Memory memory_info ;;
            4) monitor Disk disk_info ;;
            0) break ;;
            *) printf 'Choose a menu option.\n' ;;
        esac
    done
}

main
