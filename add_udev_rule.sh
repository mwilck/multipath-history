cat - <<EOF >> /etc/udev/udev.rules
# reverse mappings
KERNEL="*" SYMLINK="reverse/%M:%m"
EOF
