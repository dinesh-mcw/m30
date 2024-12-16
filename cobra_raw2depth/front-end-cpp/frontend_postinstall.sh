#!/bin/sh

if pidof grab ; then
	systemctl stop grab
	systemctl disable grab
	rm /lib/systemd/system/grab.service
fi

pidof frontend && systemctl stop frontend

systemctl daemon-reload
systemctl start frontend
systemctl enable frontend
