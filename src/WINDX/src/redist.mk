DIRECTX_REDIST_URL := https://download.microsoft.com/download/8/4/a/84a35bf1-dafe-4ae8-82af-ad2ae20b6b14/directx_Jun2010_redist.exe
DIRECTX_REDIST_SHA256 := 053f76dcbb28802e23341b6a787e3b0791c0fa5c8d4d011b1044172dbf89c73b
DIRECTX_REDIST := $(BIN)/redist/directx_Jun2010_redist.exe

.PHONY: redist
redist: $(DIRECTX_REDIST)
	printf '%s  %s\n' '$(DIRECTX_REDIST_SHA256)' '$<' | sha256sum --check

$(DIRECTX_REDIST):
	mkdir -p $(@D)
	@set -e; tmp=$$(mktemp "$@.XXXXXX"); trap 'rm -f "$$tmp"' EXIT; \
	    curl --fail --location --retry 3 --output "$$tmp" '$(DIRECTX_REDIST_URL)'; \
	    printf '%s  %s\n' '$(DIRECTX_REDIST_SHA256)' "$$tmp" | sha256sum --check; \
	    mv "$$tmp" "$@"
