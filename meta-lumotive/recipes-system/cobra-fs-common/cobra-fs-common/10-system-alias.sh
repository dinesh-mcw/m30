alias sys='systemctl --no-pager'
alias net='networkctl --no-pager'
alias log='journalctl --no-pager'
alias tcp='netstat -an | grep tcp'
alias udp='netstat -an | grep udp'
alias mess='tail -F /var/log/messages'

# System versions
alias bootv='\
    echo U-BOOT=$(dd if=/dev/mmcblk0boot0 bs=1M count=2 2>/dev/null | strings | sed -n "s/.*U-Boot[[:space:]]\([0-9]*\.[0-9]*\).*/\1/p" | head -1); \
    echo KERNEL=$(uname -r | sed -n "s/^\([0-9]*\.[0-9]*\.[0-9]*\).*/\1/p") \
'
alias versions='bootv; cat /etc/lumotive_fs_rev'
alias v=versions

# Stopwatch helper tool
alias stopwatch='s=0;f=0; while :; do { usleep 100000; let f++; [ "$f" -ge 10 ] && { f=0; let s++; }; echo -en "\r${s}.${f}"; } done'
alias s=stopwatch

# Temperature helper tool
alias temps='\
    echo -n "A53:  "; convert_temp /sys/class/thermal/thermal_zone0/temp; \
    echo -n "A72:  "; convert_temp /sys/class/thermal/thermal_zone1/temp; \
    echo -n "GPU1: "; convert_temp /sys/class/thermal/thermal_zone2/temp; \
    echo -n "GPU2: "; convert_temp /sys/class/thermal/thermal_zone3/temp; \
    echo -n "DDR:  "; convert_temp /sys/class/thermal/thermal_zone4/temp; \
'
alias t=temps

function convert_temp
{
    awk '{ printf "%d %s\n", $1/1000, "C" }' $1 2>/dev/null || echo "error"
}
