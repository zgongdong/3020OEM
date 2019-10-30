ifeq ($(OS),Windows_NT)
  copydir=xcopy /EIRYQ $1 $2
  /=\\
else
  copydir=mkdir -p $2 && cp -Tr $1 $2
  /=/
endif

.DEFAULT_GOAL := patched_all

.PHONY: patched_all install_os_hdrs
patched_all: all install_os_hdrs
	$(call copydir, ..$(/)..$(/)installed_libs$(/)lib, ..$(/)..$(/)..$(/)..$(/)installed_libs$(/)lib)

# Copy installed_libs from the hydra_os pkg folder in the branch
install_os_hdrs: $(ADK_INTERFACE)
	$(call copydir, ..$(/)..$(/)installed_libs$(/)include, ..$(/)..$(/)..$(/)..$(/)installed_libs$(/)include)