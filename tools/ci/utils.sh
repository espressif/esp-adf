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

function common_before_scripts() {
  source $IDF_PATH/tools/ci/utils.sh
  is_based_on_commits $REQUIRED_ANCESTOR_COMMITS

  if [[ -n "$IDF_DONT_USE_MIRRORS" ]]; then
    export IDF_MIRROR_PREFIX_MAP=
  fi

  if echo "$CI_MERGE_REQUEST_LABELS" | egrep "^([^,\n\r]+,)*include_nightly_run(,[^,\n\r]+)*$"; then
    export INCLUDE_NIGHTLY_RUN="1"
  fi

  # configure cmake related flags
  source $IDF_PATH/tools/ci/configure_ci_environment.sh

  # add extra python packages
  export PYTHONPATH="$IDF_PATH/tools:$IDF_PATH/tools/esp_app_trace:$IDF_PATH/components/partition_table:$IDF_PATH/tools/ci/python_packages:$PYTHONPATH"
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
  # target test jobs
  if [[ "${CI_JOB_STAGE}" == "target_test" ]]; then
    # ttfw jobs
    if ! echo "${CI_JOB_NAME}" | egrep ".+_pytest_.+"; then
      run_cmd bash install.sh --enable-ci --enable-ttfw
    else
      run_cmd bash install.sh --enable-ci --enable-pytest
    fi
  elif [[ "${CI_JOB_STAGE}" == "build_doc" ]]; then
    run_cmd bash install.sh --enable-ci --enable-docs
  elif [[ "${CI_JOB_STAGE}" == "build" ]]; then
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
  # This function prioritizes obtaining the release/${IDF_VERSION_TAG} branch based on the ${IDF_VERSION_TAG} variable.
  # If release/${IDF_TAG_FLAG} does not exist, then ${IDF_VERSION_TAG} is considered a coommit ID.
  # If ${IDF_TAG_FLAG} is true, then ${IDF_VERSION_TAG} is considered the tag version
  local idf_ver_tag="${1}"
  if [[ "$IDF_TAG_FLAG" = "true" ]]; then
    export IDF_VERSION="${idf_ver_tag}"
  else
    if [[ -x "${IDF_PATH}" ]]; then
      pushd ${IDF_PATH}
      local idf_ver=$(git ls-remote --heads origin release/${idf_ver_tag} | grep -o "release/.*")
      if [[ -n "$idf_ver" ]]; then
        export IDF_VERSION="release/${idf_ver_tag}"
      else
        export IDF_VERSION="${idf_ver_tag}"
      fi
      popd
    else
      echo "IDF_PATH not set or path not exist"
      export IDF_VERSION="${idf_ver_tag}"
    fi
  fi
  echo "IDF_TAG_FLAG: $IDF_TAG_FLAG"
  echo "Set IDF_VERSION $IDF_VERSION"
}

function set_idf() {
  # sets up the IDF repo incl submodules with specified version as $1
  #set -x # Exit if command failed.

  if [ -z $IDF_PATH ] || [ -z $IDF_VERSION ] ; then
      echo "Mandatory variables undefined"
      exit 1;
  fi;

  pushd $IDF_PATH
  # Cleans out the untracked files in the repo, so the next "git checkout" doesn't fail
  git clean -f

  if [[ "$IDF_TAG_FLAG" = "true" ]]; then
      git fetch origin tag ${IDF_VERSION} --depth 1
      git checkout ${IDF_VERSION}
      echo "The IDF branch is TAG:"${IDF_VERSION}
  else
      git fetch origin ${IDF_VERSION}:${IDF_VERSION} --depth 1
      git checkout ${IDF_VERSION}
      echo "The IDF branch is "${IDF_VERSION}
  fi

  git log -1
  # Removes the mqtt submodule, so the next submodule update doesn't fail
  rm -rf $IDF_PATH/components/mqtt/esp-mqtt
  git submodule update --init --recursive --depth 1

  popd
}

function update_submodule_remote() {
  if [[ -n "${ADF_PATH}" ]]; then
    pushd ${ADF_PATH}
    # replace submodule to internal repository to speedup cloning
    sed -i "s%https://github.com/espressif/esp-idf%${GITLAB_SSH_SERVER}/idf/esp-idf.git%" .gitmodules
    sed -i "s%https://github.com/espressif/esp-adf-libs%${GITLAB_SSH_SERVER}/adf/esp-adf-libs.git%" .gitmodules
    sed -i "s%https://github.com/espressif/esp-sr.git%${GITLAB_SSH_SERVER}/speech-recognition-framework/esp-sr.git%" .gitmodules
    popd
  else
    echo "ADF_PATH not set"
  fi
}

function fetch_idf_branch() {
  local idf_ver="${1}"
  update_submodule_remote
  check_idf_version ${idf_ver}

  if [[ -n "${IDF_PATH}" ]]; then
    pushd ${IDF_PATH}
    git init
    git clean -f
    local result=$(git remote)
    if [[ -n "$result" ]]; then
      git remote set-url origin ${GITLAB_SSH_SERVER}/espressif/esp-idf.git
    else
      git remote add origin ${GITLAB_SSH_SERVER}/espressif/esp-idf.git
    fi
    popd

    set_idf
  else
    echo "IDF_PATH not set"
  fi
}

function set_env_variable() {
  export LDGEN_CHECK_MAPPING=
  export PEDANTIC_CFLAGS="-w"
  export EXTRA_CFLAGS=${PEDANTIC_CFLAGS} && export EXTRA_CXXFLAGS=${EXTRA_CFLAGS}
  export PYTHONPATH="$ADF_PATH/tools:$PYTHONPATH"
}

function check_apps_and_filter() {
  if [[ -n "${ADF_PATH}" && -n "${IDF_TARGET}" && -n "${AUDIO_BOARD}" && -n "${IDF_VERSION_TAG}" && -n "${SDKCFG_DEFAULTS}" ]]; then
    source ${ADF_PATH}/tools/ci/apps_filter.sh ${IDF_TARGET} ${AUDIO_BOARD} ${IDF_VERSION_TAG} ${ADF_PATH}/tools/ci/apps.json
    source ${ADF_PATH}/tools/ci/check_apps_json_and_sdkcfg.sh ${SDKCFG_DEFAULTS} ${AUDIO_BOARD}
  else
    echo "Environment variables are empty"
  fi
}

function define_config_file_name() {
  # for parallel jobs, CI_JOB_NAME will be "job_name index/total" (for example, "IT_001 1/2")
  # we need to convert to pattern "job_name_index.yml"
  export JOB_NAME_PREFIX=$(echo ${CI_JOB_NAME} | awk '{print $1}')
  export JOB_FULL_NAME="${JOB_NAME_PREFIX}_${CI_NODE_INDEX}"
  export CONFIG_FILE="${CONFIG_FILE_PATH}/${JOB_FULL_NAME}.yml"
}
