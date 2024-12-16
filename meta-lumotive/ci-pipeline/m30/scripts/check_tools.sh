#!/bin/bash
set -e
set -x

# Check if ansible and ansible-playbook is installed and if it is print the version details
ansible --version
ansible-playbook --version

