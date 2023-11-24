# Wait 
sleep 1
if [[ "$(id --user)" != 0 ]]; then
    echo "Child: ERROR: Parent did not map us to root"
    exit 1
fi
ROOT=$(mktemp -d)
# We don't need to umount these at the end, as the mount namespace die with us
mount -t tmpfs tmpfs "${ROOT}" -o mode=0755,nosuid
mkdir -p "${ROOT}"/{dev/{pts,shm},etc/pacman.d,proc,var/lib/pacman,log}
mount -t proc proc "${ROOT}"/proc -o nosuid,noexec,nodev
mount -t devpts devpts "${ROOT}"/dev/pts -o mode=0620,gid=5,nosuid,noexec

for NODE in full null random tty urandom zero; do
    DEVNODE="${ROOT}"/dev/"${NODE}"
    touch "${DEVNODE}"
    mount --bind /dev/"${NODE}" "${DEVNODE}"
done

ln -s /proc/self/fd/2 "${ROOT}"/dev/stderr
ln -s /proc/self/fd/1 "${ROOT}"/dev/stdout
ln -s /proc/self/fd/0 "${ROOT}"/dev/stdin
ln -s /proc/kcore "${ROOT}"/dev/core
ln -s /proc/self/fd "${ROOT}"/dev/fd
ln -s pts/ptmx "${ROOT}"/dev/ptmx
ln -s $(readlink -f /dev/stdout) "${ROOT}"/dev/console

mirror_archlinuxarm=http://repo.lan:9129/repo/archlinuxarm
repo_url_alarm_aarch64="${mirror_archlinuxarm}"/aarch64/'$repo'

PACMAN_CONFIG="
RootDir      = ${ROOT}
DBPath       = ${ROOT}/var/lib/pacman/
CacheDir     = src/pkg/
LogFile      = ${ROOT}/var/log/pacman.log
GPGDir       = ${ROOT}/etc/pacman.d/gnupg/
HookDir      = ${ROOT}/etc/pacman.d/hooks/
Architecture = aarch64"
PACMAN_MIRRORS="
[core]
Server = ${repo_url_alarm_aarch64}
[extra]
Server = ${repo_url_alarm_aarch64}
[alarm]
Server = ${repo_url_alarm_aarch64}
[aur]
Server = ${repo_url_alarm_aarch64}"

echo "[options]${PACMAN_CONFIG}
SigLevel = Never${PACMAN_MIRRORS}" > /tmp/pacman-loose.conf

# ls -lh ${ROOT}/dev
pacman --config /tmp/pacman-loose.conf -Sy --noconfirm base-devel

ls -lh "${ROOT}"
getcap "${ROOT}"/usr/bin/newuidmap
umount --lazy "${ROOT}"
ls -lh "${ROOT}"
rm -rf "${ROOT}"