# Linux System Health and Resource Monitoring Manager

I use this Bash menu to check system resources, look up processes, and save a health report.

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
