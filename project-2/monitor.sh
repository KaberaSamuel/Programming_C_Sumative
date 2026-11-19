#!/usr/bin/env bash

export LC_ALL=C

activity_log="${SYSTEM_HEALTH_LOG:-$HOME/.system-health.log}"

log_activity() {
    check_tools date || return
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
    check_tools uname uptime date || return 1
    local os=Linux host
    if ! read -r host < /proc/sys/kernel/hostname; then
        check_tools hostname || return 1
        host=$(hostname) || return 1
    fi
    if [[ -r /etc/os-release ]]; then
        . /etc/os-release
        os=${PRETTY_NAME:-Linux}
    fi
    printf 'Hostname: %s\nOS: %s\nKernel: %s\nUptime: %s\nDate: %s\n' \
        "$host" "$os" "$(uname -r)" "$(uptime -p)" "$(date)"
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

view_processes() {
    check_tools ps awk || return
    local query processes
    read -r -p 'Enter a PID or name, or press Enter to list all: ' query || return
    processes=$(ps -eo pid,pcpu,pmem,comm) || return
    if [[ -z $query ]]; then
        printf '%s\n' "$processes"
    elif ! QUERY="$query" awk '
        NR == 1 { print; next }
        ENVIRON["QUERY"] ~ /^[0-9]+$/ {
            if ($1 == ENVIRON["QUERY"]) {print; found = 1}
            next
        }
        {
            name = $0
            sub(/^[[:space:]]*[^[:space:]]+[[:space:]]+[^[:space:]]+[[:space:]]+[^[:space:]]+[[:space:]]+/, "", name)
            if (index(tolower(name), tolower(ENVIRON["QUERY"]))) { print; found = 1 }
        }
        END {if (!found) exit 1}
    ' <<< "$processes"; then
        printf 'No matching process.\n'
    fi
    log_activity "Viewed or searched processes: ${query:-all}."
}

protected_pid() {
    local current=$$
    while (( current > 1 )); do
        if [[ $1 == "$current" ]]; then
            return 0
        fi
        current=$(ps -o ppid= -p "$current") || return 0
        read -r current <<< "$current"
        [[ $current =~ ^[0-9]+$ ]] || return 0
    done
    return 1
}

process_details() {
    ps -p "$1" -o euid=,tty=,lstart=,comm=
}

terminate_process() {
    check_tools ps || return
    local pid details owner terminal rest answer
    read -r -p 'PID to terminate: ' pid || return
    if [[ ! $pid =~ ^[1-9][0-9]{0,9}$ ]]; then
        printf 'Enter a positive PID.\n'
        log_activity 'Rejected an invalid PID.'
        return
    fi
    if [[ $pid == 1 ]] || (( EUID == 0 )) || protected_pid "$pid"; then
        printf 'This process is protected.\n'
        log_activity "Refused to terminate PID $pid."
        return
    fi
    if ! details=$(process_details "$pid") || [[ -z $details ]]; then
        printf 'Process %s was not found.\n' "$pid"
        log_activity "PID $pid was not found."
        return
    fi
    read -r owner terminal rest <<< "$details"
    if [[ $owner != "$EUID" || $terminal == '?' ]]; then
        printf 'Only your own terminal processes can be terminated.\n'
        log_activity "Refused to terminate PID $pid."
        return
    fi
    ps -p "$pid" -o pid,pcpu,pmem,tty,comm || return
    read -r -p 'Send SIGTERM? [y/N] ' answer || return
    if [[ $answer != [yY] ]]; then
        printf 'Cancelled.\n'
        log_activity "Cancelled termination of PID $pid."
    elif [[ $(process_details "$pid") != "$details" ]]; then
        printf 'The process changed or exited.\n'
        log_activity "PID $pid changed before termination."
    elif kill -TERM "$pid"; then
        printf 'Sent SIGTERM to PID %s.\n' "$pid"
        log_activity "Sent SIGTERM to PID $pid."
    else
        printf 'Could not terminate PID %s.\n' "$pid"
        log_activity "Failed to terminate PID $pid."
    fi
}

write_report() {
    system_info && printf '\n' && cpu_info && printf '\n' &&
        memory_info && printf '\n' && disk_info
}

generate_report() {
    check_tools date mktemp rm || return
    local directory report stamp
    read -r -p 'Report directory: ' directory || return
    if [[ ! -d $directory || ! -w $directory || ! -x $directory ]]; then
        printf 'Choose an existing writable directory.\n'
        log_activity 'Report directory was inaccessible.'
        return
    fi
    stamp=$(date '+%Y-%m-%d_%H-%M-%S') || return
    if ! report=$(mktemp -- "$directory/health-$stamp-XXXXXX.txt"); then
        log_activity 'Could not create a health report.'
        return
    fi
    if write_report > "$report"; then
        printf 'Saved report to %s\n' "$report"
        log_activity "Generated report: $report"
    else
        rm -- "$report"
        printf 'Could not complete the report.\n'
        log_activity 'Health report generation failed.'
    fi
}

view_history() {
    local choice
    printf '%s\n' '1. Activity log' '2. Recent system log entries'
    read -r -p 'Choose a log: ' choice || return
    case "$choice" in
        1)
            if [[ ! -s $activity_log ]]; then
                printf 'No activity has been recorded.\n'
            elif check_tools cat && ! cat -- "$activity_log"; then
                printf 'Could not read the activity log.\n'
            fi
            ;;
        2)
            check_tools journalctl || return
            journalctl -n 20 --no-pager || printf 'Could not read the system journal.\n'
            log_activity 'Requested recent system journal entries.'
            ;;
        *) printf 'Choose a log option.\n' ;;
    esac
}

clear_log() {
    local answer
    read -r -p 'Clear the activity log? [y/N] ' answer || return
    if [[ $answer != [yY] ]]; then
        printf 'Cancelled.\n'
    elif : > "$activity_log"; then
        printf 'Activity log cleared.\n'
    else
        printf 'Could not clear the activity log.\n'
    fi
}

show_menu() {
    printf '\nSystem Health Manager\n'
    printf '%s\n' '1. System information' '2. Monitor CPU' '3. Monitor memory' \
        '4. Monitor disks' '5. View or search processes' '6. Terminate a process' \
        '7. Generate a health report' '8. View logs' '9. Clear the activity log' '0. Exit'
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
            5) view_processes ;;
            6) terminate_process ;;
            7) generate_report ;;
            8) view_history ;;
            9) clear_log ;;
            0) break ;;
            *) printf 'Choose a menu option.\n' ;;
        esac
    done
}

main
