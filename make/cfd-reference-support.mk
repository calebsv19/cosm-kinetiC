# Exact macOS Accelerate support ABIs from tracked sources. No old run required.
CFD_REFERENCE_SUPPORT_STEMS := $(shell python3 scripts/cfd_reference_test_support.py --stems)
CFD_REFERENCE_SUPPORT_LIBS := $(addprefix $(abspath $(BUILD_DIR))/cfd-reference-support/,$(addsuffix .dylib,$(CFD_REFERENCE_SUPPORT_STEMS)))
.PHONY: cfd-reference-support cfd-reference-amg-env test-cfd-reference-current
cfd-reference-support: $(CFD_REFERENCE_SUPPORT_LIBS)

$(abspath $(BUILD_DIR))/cfd-reference-support/%.dylib: scripts/%.c scripts/cfd_reference_test_support.py
	@mkdir -p "$(@D)"
	@test "$(UNAME_S)" = Darwin || { echo 'These reference support ABIs require macOS Accelerate'; exit 2; }
	python3 -B scripts/atomic_output.py -- $(CC) -std=c11 -O2 $(CFD_REFERENCE_SUPPORT_FLAGS) -Wall -Wextra -Werror -dynamiclib "$<" -framework Accelerate -o "$@"


test-cfd-reference-current: cfd-reference-support
	PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 $(CFD_REFINED_REFERENCE_PYTHON) -m unittest discover -s tests -p 'test_cfd_reference3d*.py' -v

# Match the existing native-inner runners' declared CBLAS interface.
CFD_REFERENCE_CBLAS_LIBS := $(addprefix $(abspath $(BUILD_DIR))/cfd-reference-support/cfd_reference3d_,$(addsuffix .dylib,local_cost_split native_inner8 packed_inner8 packed_physical))
$(CFD_REFERENCE_CBLAS_LIBS): CFD_REFERENCE_SUPPORT_FLAGS := -DACCELERATE_NEW_LAPACK -DACCELERATE_LAPACK_ILP64
