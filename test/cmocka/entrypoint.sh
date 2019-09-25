#!/bin/bash

# Create user
USER_ID=${LOCAL_USER_ID:-9001}
useradd --shell /bin/bash -u $USER_ID -G sudo -o -c "" -m user
adduser --disabled-password user
export HOME=/home/user
echo "user ALL = NOPASSWD : ALL" >> /etc/sudoers

# Configure OpenSSH to allow login without password
sed -i -re 's/^user:[^:]+:/user::/' /etc/passwd /etc/shadow
pam_config="auth [success=1 default=ignore] pam_unix.so nullok\nauth requisite pam_deny.so\nauth required pam_permit.so"
sed -i "s/@include common-auth/$pam_config/" /etc/pam.d/sshd

# Start supervisor
/usr/bin/supervisord -c /etc/supervisor/conf.d/supervisord.conf -l /var/log/supervisord.log -j /var/run/supervisord.pid
