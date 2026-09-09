#!/bin/sh
set -eu

binary=$1
test_root=${TMPDIR:-/tmp}/onekvm-tailscale-controller-$$
trap 'rm -rf "$test_root"' EXIT HUP INT TERM
mkdir -p "$test_root/config" "$test_root/data" "$test_root/sockets"
mkdir -p "$test_root/bin"
printf '%s\n' '{"auth_key":"","hostname":"","control_url":"","accept_routes":false,"shields_up":false,"port":41641}' >"$test_root/config/config.json"

run_cli() {
    ONEKVM_EXTENSION_CONFIG="$test_root/config" \
    ONEKVM_EXTENSION_DATA="$test_root/data" \
    ONEKVM_EXTENSION_SOCKET="$test_root/sockets" \
    ONEKVM_TAILSCALE_CLI="$test_root/bin/tailscale" \
    ONEKVM_TAILSCALED="$test_root/bin/tailscaled" \
        "$binary" "$1"
}

status_output=$(run_cli status)
printf '%s' "$status_output" | grep -q '"backendState"'

connect_output=$(run_cli connect)
printf '%s' "$connect_output" | grep -q '"started":true'
grep -q . "$test_root/data/oauth-wanted"

ONEKVM_EXTENSION_CONFIG="$test_root/config" "$binary" --health

cp "$binary" "$test_root/bin/onekvm-tailscale-controller"
cat >"$test_root/bin/tailscaled" <<'EOF'
#!/bin/sh
printf '%s\n' "$@" >"$ONEKVM_EXTENSION_DATA/tailscaled-arguments"
EOF
chmod 0755 "$test_root/bin/tailscaled"
chmod 0700 "$test_root/data"
printf '%s\n' 'preserved-state' >"$test_root/data/tailscaled.state"
ONEKVM_EXTENSION_CONFIG="$test_root/config" \
ONEKVM_EXTENSION_DATA="$test_root/data" \
ONEKVM_EXTENSION_SOCKET="$test_root/sockets" \
ONEKVM_TAILSCALE_CLI="$test_root/bin/tailscale" \
ONEKVM_TAILSCALED="$test_root/bin/tailscaled" \
    "$test_root/bin/onekvm-tailscale-controller" --run
grep -Fxq -- "--state=$test_root/data/tailscaled/tailscaled.state" "$test_root/data/tailscaled-arguments"
grep -Fxq -- "--statedir=$test_root/data/tailscaled" "$test_root/data/tailscaled-arguments"
[ "$(stat -c '%a' "$test_root/data")" = 750 ]
[ "$(stat -c '%a' "$test_root/data/tailscaled")" = 700 ]
[ ! -e "$test_root/data/tailscaled.state" ]
grep -Fxq 'preserved-state' "$test_root/data/tailscaled/tailscaled.state"

cat >"$test_root/bin/tailscale" <<'EOF'
#!/bin/sh
case " $* " in
    *" status --json "*) cat "$ONEKVM_EXTENSION_DATA/status-fixture" ;;
    *" up "*)
        printf '%s\n' "$@" >"$ONEKVM_EXTENSION_DATA/up-arguments"
        printf '%s\n' '{"AuthURL":"https://login.example/new"}'
        ;;
    *" down "*|*" logout "*)
        printf '%s\n' "$@" >"$ONEKVM_EXTENSION_DATA/control-arguments"
        ;;
esac
EOF
chmod 0755 "$test_root/bin/tailscale"

: >"$test_root/sockets/tailscaled.sock"

printf '%s\n' '{"BackendState":"NeedsLogin"}' >"$test_root/data/status-fixture"
printf '%s\n' '{"AuthURL":"https://login.example/old"}' >"$test_root/data/oauth-output"
status_output=$(run_cli status)
printf '%s' "$status_output" | grep -q '"loggedIn":false'
printf '%s' "$status_output" | grep -Fq '"authURL":"https:\/\/login.example\/old"'

printf '%s\n' '{"BackendState":"Starting"}' >"$test_root/data/status-fixture"
status_output=$(run_cli status)
printf '%s' "$status_output" | grep -q '"authURL":""'

printf '%s\n' '{"BackendState":"Running","AuthURL":"https://login.example/stale","Self":{"HostName":"onekvm","TailscaleIPs":["100.64.0.1"]}}' >"$test_root/data/status-fixture"
status_output=$(run_cli status)
printf '%s' "$status_output" | grep -q '"loggedIn":true'
printf '%s' "$status_output" | grep -q '"authURL":""'
[ ! -e "$test_root/data/oauth-output" ]

printf '%s\n' '{"BackendState":"NeedsLogin","AuthURL":"https://login.example/stale"}' >"$test_root/data/status-fixture"
status_output=$(run_cli status)
printf '%s' "$status_output" | grep -q '"loggedIn":false'
printf '%s' "$status_output" | grep -q '"authURL":""'

printf '%s\n' '{"BackendState":"Stopped","Self":{"HostName":"onekvm","TailscaleIPs":["100.64.0.1"]}}' >"$test_root/data/status-fixture"
status_output=$(run_cli status)
printf '%s' "$status_output" | grep -q '"loggedIn":true'

disconnect_output=$(run_cli disconnect)
printf '%s' "$disconnect_output" | grep -q '"connected":false'
[ ! -e "$test_root/data/oauth-wanted" ]
grep -Fxq down "$test_root/data/control-arguments"

ONEKVM_EXTENSION_CONFIG="$test_root/config" \
ONEKVM_EXTENSION_DATA="$test_root/data" \
ONEKVM_EXTENSION_SOCKET="$test_root/sockets" \
ONEKVM_TAILSCALE_CLI="$test_root/bin/tailscale" \
ONEKVM_TAILSCALED="$test_root/bin/tailscaled" \
    "$test_root/bin/onekvm-tailscale-controller" --oauth
[ ! -e "$test_root/data/oauth-output" ]
! grep -q '^--hostname=' "$test_root/data/up-arguments"
