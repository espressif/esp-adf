# Workspace root for repo-relative paths. Set PROJECT_ROOT before sourcing, or use CI_PROJECT_DIR (GitLab).
: "${PROJECT_ROOT:=${CI_PROJECT_DIR:-}}"
: "${ADF_PATH:=${PROJECT_ROOT}}"
export ADF_PATH

function add_ssh_keys() {
  local key_string="${1}"
  mkdir -p ~/.ssh
  chmod 700 ~/.ssh
  echo -n "${key_string}" >~/.ssh/id_rsa_base64
  base64 --decode --ignore-garbage ~/.ssh/id_rsa_base64 >~/.ssh/id_rsa
  chmod 600 ~/.ssh/id_rsa
}

function add_gitlab_ssh_keys() {
  add_ssh_keys "${GITLAB_KEY}"
  echo -e "Host gitlab.espressif.cn\n\tStrictHostKeyChecking no\n" >>~/.ssh/config

  # For gitlab geo nodes
  if [ "${LOCAL_GITLAB_SSH_SERVER:-}" ]; then
    SRV=${LOCAL_GITLAB_SSH_SERVER##*@} # remove the chars before @, which is the account
    SRV=${SRV%%:*}                     # remove the chars after :, which is the port
    printf "Host %s\n\tStrictHostKeyChecking no\n" "${SRV}" >>~/.ssh/config
  fi
}

function add_github_ssh_keys() {
  add_ssh_keys "${GH_PUSH_KEY}"
  echo -e "Host github.com\n\tStrictHostKeyChecking no\n" >>~/.ssh/config
}

function add_doc_server_ssh_keys() {
  local key_string="${1}"
  local server_url="${2}"
  local server_user="${3}"
  add_ssh_keys "${key_string}"
  echo -e "Host ${server_url}\n\tStrictHostKeyChecking no\n\tUser ${server_user}\n" >>~/.ssh/config
}

function configure_ci_env() {
  source $IDF_PATH/tools/ci/utils.sh
  is_based_on_commits $REQUIRED_ANCESTOR_COMMITS

  if [[ -n "$IDF_DONT_USE_MIRRORS" ]]; then
    export IDF_MIRROR_PREFIX_MAP=
  fi

  if echo "$CI_MERGE_REQUEST_LABELS" | egrep "^([^,\n\r]+,)*include_nightly_run(,[^,\n\r]+)*$"; then
    export INCLUDE_NIGHTLY_RUN="1"
  fi

  # configure cmake related flags (GitLab sets CI_COMMIT_REF_NAME; skip locally when unset)
  if [ -n "${CI_COMMIT_REF_NAME:-}" ]; then
    source "$IDF_PATH/tools/ci/configure_ci_environment.sh"
  fi

  set_env_variable
}

function error() {
  printf "\033[0;31m%s\n\033[0m" "${1}" >&2
}

function info() {
  printf "\033[0;32m%s\n\033[0m" "${1}" >&2
}

function warning() {
  printf "\033[0;33m%s\n\033[0m" "${1}" >&2
}

function run_cmd() {
  local start=$(date +%s)
  info "\$ $*"
  "$@"
  local ret=$?
  local end=$(date +%s)
  local runtime=$((end-start))

  if [[ $ret -eq 0 ]]; then
    info "==> '\$ $*' succeeded in ${runtime} seconds."
    return 0
  else
    error "==> '\$ $*' failed (${ret}) in ${runtime} seconds."
    return $ret
  fi
}

# Echo IDF major version from ${IDF_PATH}/tools/cmake/version.cmake, or empty if unknown.
function _utils_idf_major_version() {
  local vf="${IDF_PATH}/tools/cmake/version.cmake"
  [[ -n "${IDF_PATH}" && -f "$vf" ]] || { echo ''; return; }
  grep 'IDF_VERSION_MAJOR' "$vf" | head -1 | sed -e 's/.*IDF_VERSION_MAJOR[[:space:]]*\([0-9][0-9]*\).*/\1/'
}

function setup_tools_and_idf_python_venv() {
  # must use after setup_tools_except_target_test
  # otherwise the export.sh won't work properly
  cd $IDF_PATH

  # download constraint file for dev
  if [[ -n "$CI_PYTHON_CONSTRAINT_BRANCH" ]]; then
    wget -O /tmp/constraint.txt --header="Authorization:Bearer ${ESPCI_TOKEN}" ${GITLAB_HTTP_SERVER}/api/v4/projects/2581/repository/files/${CI_PYTHON_CONSTRAINT_FILE}/raw?ref=${CI_PYTHON_CONSTRAINT_BRANCH}
    mkdir -p ~/.espressif
    mv /tmp/constraint.txt ~/.espressif/${CI_PYTHON_CONSTRAINT_FILE}
  fi

  # Mirror
  if [[ -n "$IDF_DONT_USE_MIRRORS" ]]; then
    export IDF_MIRROR_PREFIX_MAP=
  fi

  # install latest python packages
  local _idf_major
  _idf_major=$(_utils_idf_major_version)
  if [[ "${CI_JOB_STAGE}" == "build_doc" ]]; then
    run_cmd bash install.sh --enable-ci --enable-docs
  elif [[ ( "${CI_JOB_STAGE}" == "target_test" || "${CI_JOB_STAGE}" == "build" ) && ( -z "${_idf_major}" || "${_idf_major}" -lt 6 ) ]]; then
    run_cmd bash install.sh --enable-ci --enable-pytest
  else
    run_cmd bash install.sh --enable-ci
  fi

  source ./export.sh

  # Custom OpenOCD
  if [[ ! -z "$OOCD_DISTRO_URL" && "$CI_JOB_STAGE" == "target_test" ]]; then
    echo "Using custom OpenOCD from ${OOCD_DISTRO_URL}"
    wget $OOCD_DISTRO_URL
    ARCH_NAME=$(basename $OOCD_DISTRO_URL)
    tar -x -f $ARCH_NAME
    export OPENOCD_SCRIPTS=$PWD/openocd-esp32/share/openocd/scripts
    export PATH=$PWD/openocd-esp32/bin:$PATH
  fi

  if [[ -n "$CI_PYTHON_TOOL_REPO" ]]; then
    git clone --quiet --depth=1 -b ${CI_PYTHON_TOOL_BRANCH} https://gitlab-ci-token:${ESPCI_TOKEN}@${GITLAB_HTTPS_HOST}/espressif/${CI_PYTHON_TOOL_REPO}.git
    pip install ./${CI_PYTHON_TOOL_REPO}
    rm -rf ${CI_PYTHON_TOOL_REPO}
  fi
  cd -
}

function check_idf_version() {
  # Resolve IDF_VERSION_TAG -> IDF_VERSION. The fetch stage always uses _git_shallow_fetch_checkout,
  # which supports branch names, arbitrary tag names, commit SHAs, and refs/heads|tags/... prefixes.
  local idf_ver_tag="${1}"

  # Check if it's a release branch (starts with "release/")
  if [[ "$idf_ver_tag" =~ ^release/ ]]; then
    export IDF_VERSION="${idf_ver_tag}"
    echo "Detected release branch: ${IDF_VERSION}"
  # Typical version tag (not exhaustive: custom tags fall through to branch/commit logic below)
  elif [[ "$idf_ver_tag" =~ ^v[0-9]+\.[0-9]+(\.[0-9]+)?$ ]]; then
    export IDF_VERSION="${idf_ver_tag}"
    echo "Detected tag: ${IDF_VERSION}"
  elif [[ "$idf_ver_tag" =~ ^[0-9a-fA-F]{7,40}$ ]]; then
    export IDF_VERSION="${idf_ver_tag}"
    echo "Detected commit id: ${IDF_VERSION}"
  else
    # Short name may be a release/x branch, an arbitrary tag, or treated as a commit (fetch failure will clarify)
    local idf_remote="${IDF_GIT_URL:-${GITLAB_SSH_SERVER}/espressif/esp-idf.git}"
    if [[ -d "${IDF_PATH}/.git" ]]; then
      pushd ${IDF_PATH} > /dev/null
      local idf_ver=$(git ls-remote --heads origin release/${idf_ver_tag} 2>/dev/null | grep -o "release/.*" | head -1)
      if [[ -n "$idf_ver" ]]; then
        export IDF_VERSION="release/${idf_ver_tag}"
        echo "Detected release branch (auto-prefixed): ${IDF_VERSION}"
      else
        export IDF_VERSION="${idf_ver_tag}"
        echo "Detected commit ID: ${IDF_VERSION}"
      fi
      popd > /dev/null
    else
      if git ls-remote "${idf_remote}" "refs/heads/release/${idf_ver_tag}" &>/dev/null; then
        export IDF_VERSION="release/${idf_ver_tag}"
        echo "Detected release branch (via ls-remote, no local idf): ${IDF_VERSION}"
      else
        export IDF_VERSION="${idf_ver_tag}"
        echo "Treating as commit ID (no local idf): ${IDF_VERSION}"
      fi
    fi
  fi
  echo "Set IDF_VERSION: ${IDF_VERSION}"
}

# Shallow-fetch and checkout an arbitrary ref in the current directory (must already be a git repo with a remote configured).
# $1: remote name (default: origin); $2: ref — full refs/..., branch name, tag name, or commit SHA (full/short).
function _git_shallow_fetch_checkout() {
  local remote="${1:-origin}"
  local ref="$2"
  if [[ -z "${ref}" ]]; then
    echo "_git_shallow_fetch_checkout: empty ref"
    return 1
  fi

  # Full refs/... path: fetch directly
  if [[ "${ref}" == refs/* ]]; then
    if git fetch --depth 1 "${remote}" "${ref}"; then
      git checkout -f FETCH_HEAD
      echo "Checked out FETCH_HEAD from ${ref}"
      return 0
    fi
    echo "failed: git fetch --depth 1 ${remote} ${ref}"
    return 1
  fi

  # Let Git resolve the ref (branch, tag, release/x, 40/short SHA)
  if git fetch --depth 1 "${remote}" "${ref}"; then
    git checkout -f FETCH_HEAD
    echo "Checked out FETCH_HEAD (resolved ref: ${ref})"
    return 0
  fi

  # Explicit branch then tag fallback (avoids ambiguity with same-name refs or older git behaviour)
  if git fetch --depth 1 "${remote}" "refs/heads/${ref}"; then
    git checkout -f FETCH_HEAD
    echo "Checked out FETCH_HEAD from branch refs/heads/${ref}"
    return 0
  fi

  if git fetch --depth 1 "${remote}" "refs/tags/${ref}"; then
    git checkout -f FETCH_HEAD
    echo "Checked out FETCH_HEAD from tag refs/tags/${ref}"
    return 0
  fi

  echo "failed to shallow-fetch ref '${ref}' from ${remote} (tried loose ref, refs/heads/, refs/tags/)"
  return 1
}

function set_idf() {
  # Switch an existing IDF repo to ${IDF_VERSION} (branch / tag / commit SHA, shallow history)

  if [ -z "${IDF_PATH}" ] || [ -z "${IDF_VERSION}" ]; then
    echo "Mandatory variables undefined"
    exit 1
  fi

  local idf_url="${IDF_GIT_URL:-${GITLAB_SSH_SERVER}/espressif/esp-idf.git}"
  if [[ -z "${idf_url}" ]]; then
    echo "set_idf: set IDF_GIT_URL or GITLAB_SSH_SERVER (esp-idf remote URL)"
    exit 1
  fi

  pushd "${IDF_PATH}" >/dev/null || exit 1
  # Cleans out the untracked files in the repo, so the next "git checkout" doesn't fail
  git clean -f

  git remote set-url origin "${idf_url}"

  if ! _git_shallow_fetch_checkout origin "${IDF_VERSION}"; then
    popd >/dev/null
    exit 1
  fi
  echo "IDF at: $(git rev-parse --short HEAD) (${IDF_VERSION})"
  popd >/dev/null
}

function update_submodule_remote() {
  if [[ -z "${PROJECT_ROOT}" ]]; then
    echo "PROJECT_ROOT not set"
    return
  fi
  pushd "${PROJECT_ROOT}" > /dev/null || return
  # esp-idf is no longer a submodule; redirect GitHub upstreams in .gitmodules to GitLab mirrors (requires GITLAB_SSH_SERVER)
  [[ -f .gitmodules ]] || { popd > /dev/null; return 0; }
  if [[ -n "${GITLAB_SSH_SERVER:-}" ]]; then
    _apply_gitmodules_mirror_pair "https://github.com/espressif/esp-adf-libs" "${GITLAB_SSH_SERVER}/adf/esp-adf-libs.git"
    _apply_gitmodules_mirror_pair "https://github.com/espressif/esp-sr.git" "${GITLAB_SSH_SERVER}/speech-recognition-framework/esp-sr.git"
  fi
  popd > /dev/null
}

# Replace first pattern in .gitmodules when both from/to are non-empty and from appears in the file.
function _apply_gitmodules_mirror_pair() {
  local from="$1"
  local to="$2"
  if [[ -n "${from}" && -n "${to}" ]] && grep -qF "${from}" .gitmodules 2>/dev/null; then
    sed -i "s%${from}%${to}%g" .gitmodules
  fi
}

function update_submodule() {
  if [[ -n "${PROJECT_ROOT}" ]]; then
    pushd "${PROJECT_ROOT}" > /dev/null
    git submodule update --init --depth 1
    popd > /dev/null
  else
    echo "PROJECT_ROOT not set"
  fi
}

# Shallow-clone esp-idf (not a submodule) into ${IDF_PATH}. Falls back to fetch/checkout via set_idf if .git already exists.
# ${IDF_VERSION} may be a branch name, tag name, commit id, or refs/heads/... / refs/tags/... prefix.
function _ensure_idf_shallow_clone() {
  local idf_url="${IDF_GIT_URL:-${GITLAB_SSH_SERVER}/espressif/esp-idf.git}"
  if [[ -z "${IDF_PATH}" || -z "${IDF_VERSION}" ]]; then
    echo "IDF_PATH or IDF_VERSION empty"
    exit 1
  fi
  if [[ -d "${IDF_PATH}/.git" ]]; then
    set_idf
    return
  fi
  rm -rf "${IDF_PATH}"
  mkdir -p "${IDF_PATH}"
  pushd "${IDF_PATH}" >/dev/null || exit 1
  info "shallow init esp-idf: ${idf_url} @ ${IDF_VERSION}"
  git init
  git remote add origin "${idf_url}"
  if ! _git_shallow_fetch_checkout origin "${IDF_VERSION}"; then
    popd >/dev/null
    exit 1
  fi
  popd >/dev/null
}

function fetch_idf_branch() {
  local idf_ver="${1}"
  local want_ref=""

  update_submodule_remote

  # Logic:
  # - Non-"default" argument: resolve and ensure shallow clone/switch to that ref
  # - "default" argument:
  #   - If IDF_VERSION_TAG is set: resolve and ensure shallow clone/switch
  #   - Otherwise: require an existing local IDF repo and use the current HEAD as-is
  if [[ "${idf_ver}" != "default" ]]; then
    want_ref="${idf_ver}"
  elif [[ -n "${IDF_VERSION_TAG:-}" ]]; then
    want_ref="${IDF_VERSION_TAG}"
  fi

  if [[ -n "${want_ref}" ]]; then
    check_idf_version "${want_ref}"
    _ensure_idf_shallow_clone
  else
    if [[ ! -d "${IDF_PATH}/.git" ]]; then
      echo "error: esp-idf not at ${IDF_PATH}; fetch_idf_branch default needs IDF_VERSION_TAG or existing clone"
      exit 1
    fi
  fi

  pushd "${IDF_PATH}" >/dev/null || exit 1
  git log -1
  rm -rf "${IDF_PATH}/components/mqtt/esp-mqtt"
  if [[ "${CI_IDF_SUBMODULE_USE_ARCHIVES:-0}" == "1" ]]; then
    python "${CI_TOOLS_PATH}/ci/git/ci_fetch_submodule.py" -s all
  else
    git submodule update --init --recursive --depth 1
  fi
  popd >/dev/null
}

function set_env_variable() {
  export LDGEN_CHECK_MAPPING=
  export PEDANTIC_CFLAGS="-w"
  export EXTRA_CFLAGS=${PEDANTIC_CFLAGS} && export EXTRA_CXXFLAGS=${EXTRA_CFLAGS}
  export PYTHONPATH="$IDF_PATH/tools:$IDF_PATH/tools/esp_app_trace:$IDF_PATH/components/partition_table:$IDF_PATH/tools/ci/python_packages:$PYTHONPATH"
  export PYTHONPATH="$CI_TOOLS_PATH:$PYTHONPATH"

}

function check_sdkconfig() {
  if [[ -n "${PROJECT_ROOT}" && -n "${IDF_TARGET}" && -n "${BOARD}" && -n "${IDF_VERSION_TAG}" && -n "${CI_TOOLS_PATH}" ]]; then
    # Run scanner first to produce apps.txt
    run_cmd python ${CI_TOOLS_PATH}/ci/app_service/apps_service.py scanner --target ${IDF_TARGET} \
              --board ${BOARD} \
              --idf_ver ${IDF_VERSION_TAG} \
              --apps-yaml ${PROJECT_ROOT}/tools/ci/apps.yaml \
              --ci-config-file "${PROJECT_ROOT}/.gitlab/ci/ci-config.yml" \
              --project-path "${PROJECT_ROOT}" \
              --apps_txt apps.txt \
              --output apps_dependent.yml
    
    # Then check sdkconfig
    run_cmd python ${CI_TOOLS_PATH}/ci/checker/check_sdkconfig.py --apps-txt apps.txt
  else
    echo "Environment variables are empty"
  fi
}

# Derive app directories from MR/commit changed-file paths (examples/ and components/.../test_apps/),
# then verify that pytest_*.py files exist. Does not rely on apps_dependent.yml.
function check_pytest() {
  if [[ -n "${PROJECT_ROOT}" && -n "${CI_TOOLS_PATH}" ]]; then
    run_cmd python ${CI_TOOLS_PATH}/ci/checker/check_pytest.py --project-root "${PROJECT_ROOT}" --allow-empty
  else
    echo "Environment variables are empty (need PROJECT_ROOT)"
  fi
}

# Resolve the apps_path.json used by child jobs.
# Priority:
# 1. APPS_PATH_JSON
# 2. ${PROJECT_ROOT}/${IDF_VERSION_TAG}/apps_path.json
# 3. ${PROJECT_ROOT}/apps_path.json
function _resolve_ci_apps_path_json() {
  if [[ -n "${APPS_PATH_JSON:-}" && -f "${APPS_PATH_JSON}" ]]; then
    printf '%s\n' "${APPS_PATH_JSON}"
    return 0
  fi
  if [[ -z "${PROJECT_ROOT:-}" ]]; then
    echo '[ERROR] PROJECT_ROOT is not set; cannot locate apps_path.json' >&2
    return 1
  fi
  if [[ -n "${IDF_VERSION_TAG:-}" ]]; then
    local per_ver="${PROJECT_ROOT}/${IDF_VERSION_TAG}/apps_path.json"
    if [[ -f "${per_ver}" ]]; then
      printf '%s\n' "${per_ver}"
      return 0
    fi
  fi
  local at_root="${PROJECT_ROOT}/apps_path.json"
  if [[ -f "${at_root}" ]]; then
    printf '%s\n' "${at_root}"
    return 0
  fi
  echo "[ERROR] apps_path.json not found (tried APPS_PATH_JSON, \${PROJECT_ROOT}/\${IDF_VERSION_TAG}/, and repo root). Ensure the generate-trigger-config artifact has been downloaded." >&2
  return 1
}

# Extract app_dir entries from apps_path.json.
# All arguments are optional; omitting or leaving empty skips that filter dimension.
#   $1  Path to apps_path.json
#   $2  app_type: example / test_app; "all" or empty means no app_type filter
#   $3  target:   e.g. esp32s3  (--match-target)
#   $4  board:    e.g. ESP32_S3_KORVO2_V3  (--match-board)
# Output: deduplicated app_dir paths in order of appearance, one per line on stdout (suitable for mapfile).
function _read_apps_path_json_paths() {
  local file="$1"
  local type_filter="${2:-}"
  local target_filter="${3:-}"
  local board_filter="${4:-}"
  [[ "${type_filter}" == "all" ]] && type_filter=""
  [[ -f "$file" ]] || return 0
  local -a cmd=(
    python "${CI_TOOLS_PATH}/ci/app_service/apps_filter.py"
    --print-matching-app-dirs
    --apps-path-file "$file"
  )
  [[ -n "$type_filter" ]] && cmd+=(--app-type "$type_filter")
  [[ -n "$target_filter" ]] && cmd+=(--match-target "$target_filter")
  [[ -n "$board_filter" ]] && cmd+=(--match-board "$board_filter")
  "${cmd[@]}"
}

# Optional $1: app_type — example / test_app / all; omitted or "all" means no app_type filter.
# Build list is taken from apps_path.json produced by the parent pipeline's generate-trigger-config job.
function build_apps() {
  local app_type="${1:-}"
  local apps_json
  apps_json="$(_resolve_ci_apps_path_json)" || exit 1
  local paths=()
  mapfile -t paths < <(_read_apps_path_json_paths "${apps_json}" "${app_type}" "${IDF_TARGET}" "${BOARD}")
  if [[ ${#paths[@]} -eq 0 ]]; then
    echo '[OK] No apps in apps_path.json match the current IDF_TARGET / BOARD / app_type; skipping build.' >&2
    return 0
  fi
  # idf_build_apps / apps_build do not pass config_rules by default; CI explicitly passes the same three rules as the old implicit default.
  # Note: paths must come before --config-rules, otherwise nargs='+' will absorb them as rule fragments.
  run_cmd python "${CI_TOOLS_PATH}/ci/app_service/apps_service.py" build \
    "${paths[@]}" \
    --config-rules 'sdkconfig.ci=default' 'sdkconfig.ci.*=' '=default' \
    -t "${IDF_TARGET}" -vv \
    --build-dir "${IDF_VERSION_TAG}/build_@t_@w" \
    --parallel-count "${CI_NODE_TOTAL:-1}" \
    --parallel-index "${CI_NODE_INDEX:-1}"
}

# Same arguments as build_apps
function pytest_apps() {
  local app_type="${1:-}"
  local apps_json
  apps_json="$(_resolve_ci_apps_path_json)" || exit 1
  job_tags="$(python "${CI_TOOLS_PATH}/ci/git/gitlab_api.py" get_job_tags "$CI_PROJECT_ID" --job_id "$CI_JOB_ID")"
  markers="$(echo "$job_tags" | sed -e 's/,/ and /g')"
  local pytest_paths=()
  mapfile -t pytest_paths < <(_read_apps_path_json_paths "${apps_json}" "${app_type}" "${IDF_TARGET}" "${BOARD}")
  [[ ${#pytest_paths[@]} -eq 0 ]] && exit 0
  run_cmd pytest "${pytest_paths[@]}" --target "${IDF_TARGET}" -m "${markers}" --junitxml=XUNIT_RESULT.xml --parallel-count 1 --parallel-index 1
}

function define_config_file_name() {
  # for parallel jobs, CI_JOB_NAME will be "job_name index/total" (for example, "IT_001 1/2")
  # we need to convert to pattern "job_name_index.yml"
  export JOB_NAME_PREFIX=$(echo ${CI_JOB_NAME} | awk '{print $1}')
  export JOB_FULL_NAME="${JOB_NAME_PREFIX}_${CI_NODE_INDEX}"
  export CONFIG_FILE="${CONFIG_FILE_PATH}/${JOB_FULL_NAME}.yml"
}

# Same as build_apps, but defaults app_type to "test_app" when $1 is not supplied.
function build_test_apps() {
  local app_type="${1:-test_app}"
  local apps_json
  # Suppress duplicate "file not found" errors on the fallback path; only the WARN below is printed
  if apps_json="$(_resolve_ci_apps_path_json 2>/dev/null)"; then
    build_apps "${app_type}"
    return $?
  fi
  echo '[WARN] apps_path.json not found; falling back to local scanner + filter + build (no child pipeline artifact).' >&2
  local apps_list_file="${PROJECT_ROOT}/apps_path.json"
  local -a svc=(
    python "${CI_TOOLS_PATH}/ci/app_service/apps_service.py"
    --target "${IDF_TARGET}"
    --board "${BOARD}"
    --idf_ver "${IDF_VERSION_TAG}"
    --app-type "${app_type}"
    --apps-yaml "${PROJECT_ROOT}/tools/ci/apps.yaml"
    --ci-config-file "${PROJECT_ROOT}/.gitlab/ci/ci-config.yml"
    --apps_txt apps.txt
    --project-path "${PROJECT_ROOT}"
    --output "${apps_list_file}"
    --export-type build
    --stop-after-filter
  )
  run_cmd "${svc[@]}"
  local paths=()
  mapfile -t paths < <(_read_apps_path_json_paths "${apps_list_file}" "${app_type}" "${IDF_TARGET}" "${BOARD}")
  [[ ${#paths[@]} -eq 0 ]] && exit 0
  run_cmd python "${CI_TOOLS_PATH}/ci/app_service/apps_service.py" build \
    "${paths[@]}" \
    --config-rules 'sdkconfig.ci=default' 'sdkconfig.ci.*=' '=default' \
    -t "${IDF_TARGET}" -vv \
    --build-dir "${IDF_VERSION_TAG}/build_@t_@w"
}

function git_clone_ci_tools() {
  if [[ -n "${CI_TOOLS_BRANCH:-}" ]]; then
    git clone -b "${CI_TOOLS_BRANCH}" "${CI_TOOLS_GIT_URL}" "${CI_TOOLS_PATH}" --depth 1
  else
    git clone "${CI_TOOLS_GIT_URL}" "${CI_TOOLS_PATH}" --depth 1
  fi
}

function before_script() {
  add_gitlab_ssh_keys
  update_submodule_remote
  update_submodule
}

function common_script() {
  git_clone_ci_tools
  fetch_idf_branch ${IDF_VERSION_TAG}
  configure_ci_env
  setup_tools_and_idf_python_venv
  ci_evn_variables_export
}

function ci_evn_variables_export() {
  # Clear the .env file
  > .env

  # Variables to export
  local variables=(
    "CI_PROJECT_ROOT"
    "PROJECT_ROOT"
    "IDF_PATH"
    "ADF_PATH"
    "BOARD"
    "IDF_TARGET"
    "IDF_VERSION_TAG"
    "GITLAB_SSH_SERVER"
  )

  # Write each non-empty variable into .env (used by target-test jobs)
  for var in "${variables[@]}"; do
    if [ -n "${!var}" ]; then
      echo "export ${var}=\"${!var}\"" >> .env
    fi
  done

  echo 'echo "export GITLAB_KEY=<*YOUR_GITLAB_KEY*>"' >> .env
}
