# Central project naming — edit only here.
#
#   PROJECT_ID     user (shell prompt, kernel artifact prefix) — e.g. d273liu
#   PROJECT_NAME   hostname / OS name (banners, Docker images) — e.g. smpos-nix (canonical APU-line)
#
# Shell scripts:  source ./source_project.sh
# C kernel code:   #include "project.h"  (generated in src/ by `make`)

PROJECT_ID   := d273liu
PROJECT_NAME := darcyos-apu

KERNEL_ELF     := os/0-$(PROJECT_ID).elf
KERNEL_IMG     := os/0-$(PROJECT_ID).img
DOCKER_DEV_IMG := $(PROJECT_NAME)-dev
DOCKER_PROD_IMG := $(PROJECT_NAME)-prod
DOCKER_WEB_CTR := $(PROJECT_ID)-web

PROJECT_H := src/layer1-processes/project.h

.PHONY: print-env
print-env:
	@echo "PROJECT_ID=$(PROJECT_ID)"
	@echo "PROJECT_NAME=$(PROJECT_NAME)"
	@echo "KERNEL_ELF=$(KERNEL_ELF)"
	@echo "KERNEL_IMG=$(KERNEL_IMG)"
	@echo "DOCKER_DEV_IMG=$(DOCKER_DEV_IMG)"
	@echo "DOCKER_PROD_IMG=$(DOCKER_PROD_IMG)"
	@echo "DOCKER_WEB_CTR=$(DOCKER_WEB_CTR)"
