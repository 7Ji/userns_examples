RUID=$(id --user)
RGID=$(id --group)
if [[ "${RUID}" == 0 ]]; then
    echo "ERROR: Not allowed to run as root (RUID = 0)"
    exit 1
fi
if [[ "${RGID}" == 0 ]]; then
    echo "ERROR: Not allowed to run as RGID = 0"
    exit 1
fi

unshare --user --pid --mount --fork bash -e child.sh &
CHILD=$!
newuidmap "${CHILD}" 0 "${RUID}" 1 1 100000 65536
newgidmap "${CHILD}" 0 "${RGID}" 1 1 100000 65536
if ! wait "${CHILD}"; then
    echo "Child bad return"
fi