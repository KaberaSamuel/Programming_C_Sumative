# Linux System Health and Resource Monitoring Manager

I use this Bash menu to check system resources, look up processes, and save a health report.

## Demonstration

I check CPU, memory, and disk usage, then find and stop a temporary process.

From this folder, start a temporary process and use a separate folder for the demo log and report.

```bash
demo_dir=$(mktemp -d)
sleep 120 &
demo_pid=$!
echo "$demo_pid"
echo "$demo_dir"
SYSTEM_HEALTH_LOG="$demo_dir/activity.log" ./monitor.sh
```

1. Choose **2. Monitor CPU** and enter `100`. Choose **3. Monitor memory** and enter `0` to see a warning. Choose **4. Monitor disks** and enter `100`.
2. Choose **5. View or search processes** and enter the printed PID. Choose **6. Terminate a process**, enter the same PID, and confirm with `y`.
3. Choose **7. Generate a health report**. Enter the folder printed by `echo "$demo_dir"`. After exiting, open the generated report with `cat "$demo_dir"/health-*.txt`.

![VS Code terminal showing CPU, memory, disk usage, and the temporary process result](demo/terminal.png)

### Terminal output

```text
System health report (excerpt)
CPU usage: 1%
Memory (MiB): 15691 total, 4814 used, 10877 available
Memory usage: 30.7%
Filesystem      Size  Used Avail Use% Mounted on
/dev/nvme0n1p4  196G  127G   60G  68% /

Warning: memory usage is above 0%.
  21087  0.0  0.0 sleep
Sent SIGTERM to PID 21087.
```

### Supporting files

- [Generated health report](demo/health-report.txt)
- [Activity log excerpt](demo/activity-log.txt)

## Run it

Open a Linux terminal in this folder and run:

```bash
./monitor.sh
```

The script uses Bash, `/proc`, and standard Linux commands such as `ps`, `free`, and `df`. System journal viewing uses `journalctl` if it is installed and you have access.

## How it works

- I read `/proc/stat` twice, one second apart, to measure CPU use. The process list shows average CPU use from `ps`.
- I use `free` for memory and `df` for disk space. Each monitor asks for a threshold from 0 to 100.
- Process search takes an exact PID or part of a process name. The script checks ownership, the terminal, and protected PIDs before asking to send SIGTERM. It checks the process again after confirmation.
- Reports go into an existing writable folder that you choose. The activity log is saved to `~/.system-health.log` by default. Set `SYSTEM_HEALTH_LOG` to use a different file.
