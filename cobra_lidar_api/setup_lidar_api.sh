#! /bin/bash
rm -rf /etc/nginx/sites-enabled/default /etc/nginx/sites-available/default
cp /home/lumotive/.local/lib/python3.10/site-packages/cobra_lidar_api/service/cb_api.service /lib/systemd/system/cb_api.service
cp /home/lumotive/.local/lib/python3.10/site-packages/cobra_lidar_api/service/boot_good.service /lib/systemd/system/boot_good.service
cp /home/lumotive/.local/lib/python3.10/site-packages/cobra_lidar_api/service/cobra_api /etc/nginx/sites-available/default
cp /home/lumotive/.local/lib/python3.10/site-packages/cobra_lidar_api/service/nginx.conf /etc/nginx/nginx.conf
ln -s /etc/nginx/sites-available/default /etc/nginx/sites-enabled/default
nginx -t
systemctl daemon-reload
systemctl enable cb_api
systemctl start cb_api
systemctl enable boot_good
systemctl reload nginx
