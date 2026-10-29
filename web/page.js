(() => {
  const runtime = window.OneKVMPluginUI.v1
  const { computed, defineComponent, h, onBeforeUnmount, onMounted, ref } = runtime.vue
  const { NAlert, NButton, NInput, NInputNumber, NModal, NSpin, NSwitch, NTag, useMessage } = runtime.naive
  const labels = runtime.i18n.createMessages({
    'zh-TW': { keyConnect: '使用金鑰連線', pasteKey: '貼上驗證金鑰', connect: '連線', connecting: '正在連線…', cancel: '取消', connectionFailed: '連線失敗，請檢查金鑰與伺服器設定後重試。', connectionTimeout: '連線等待逾時，請取消後重試。', saveFailed: '儲存失敗，請重試。', connected: '已連線。', title: 'Tailscale', status: '連線狀態', offline: 'Tailscale 服務尚未就緒', ips: 'Tailscale IP', noIP: '尚未分配 IP', browserConnect: '使用瀏覽器連線', starting: '正在準備登入…', loginTimeout: '取得登入連結逾時，請重試。', openLogin: '開啟登入頁面', copyURL: '複製登入連結', copied: '登入連結已複製。', connectStarted: '瀏覽器登入已啟動。', disconnect: '中斷連線', logout: '登出裝置', disconnected: '已中斷 Tailscale 連線。', loggedOut: '已登出 Tailscale。', refresh: '重新整理', settings: '連線設定', authKey: '驗證金鑰', controlURL: '控制伺服器 URL（可選）', port: 'UDP 連接埠', routes: '接受廣播路由', shields: '阻止傳入連線', save: '儲存設定', saved: 'Tailscale 設定已儲存。' },
    zh: { keyConnect: '使用密钥连接', pasteKey: '粘贴认证密钥', connect: '连接', connecting: '正在连接…', cancel: '取消', connectionFailed: '连接失败，请检查密钥与服务器设置后重试。', connectionTimeout: '连接等待超时，请取消后重试。', saveFailed: '保存失败，请重试。', connected: '已连接。', title: 'Tailscale', status: '连接状态', offline: 'Tailscale 服务尚未就绪', ips: 'Tailscale IP', noIP: '尚未分配 IP', browserConnect: '使用浏览器连接', starting: '正在准备登录…', loginTimeout: '获取登录链接超时，请重试。', openLogin: '打开登录页面', copyURL: '复制登录链接', copied: '登录链接已复制。', connectStarted: '浏览器登录已启动。', disconnect: '断开连接', logout: '退出登录', disconnected: '已断开 Tailscale 连接。', loggedOut: '已退出 Tailscale 登录。', refresh: '刷新', settings: '连接设置', authKey: '认证密钥', controlURL: '控制服务器 URL（可选）', port: 'UDP 端口', routes: '接受广播路由', shields: '阻止传入连接', save: '保存设置', saved: 'Tailscale 设置已保存。' },
    en: { keyConnect: 'Connect with key', pasteKey: 'Paste authentication key', connect: 'Connect', connecting: 'Connecting…', cancel: 'Cancel', connectionFailed: 'Connection failed. Check the key and server settings, then try again.', connectionTimeout: 'Timed out waiting for a connection. Cancel and try again.', saveFailed: 'Could not save settings. Please try again.', connected: 'Connected.', title: 'Tailscale', status: 'Connection status', offline: 'The Tailscale service is not ready', ips: 'Tailscale IPs', noIP: 'No IP address assigned yet', browserConnect: 'Connect in browser', starting: 'Preparing sign-in…', loginTimeout: 'Timed out waiting for the sign-in link. Please try again.', openLogin: 'Open sign-in page', copyURL: 'Copy sign-in link', copied: 'Sign-in link copied.', connectStarted: 'Browser sign-in started.', disconnect: 'Disconnect', logout: 'Log out device', disconnected: 'Tailscale disconnected.', loggedOut: 'Tailscale logged out.', refresh: 'Refresh', settings: 'Connection settings', authKey: 'Authentication key', controlURL: 'Control server URL (optional)', port: 'UDP port', routes: 'Accept advertised routes', shields: 'Block incoming connections', save: 'Save settings', saved: 'Tailscale settings saved.' },
  })

  // Keep the dialog presentational; the page owns saving and connection state.
  const KeyConnectDialog = defineComponent({
    name: 'TailscaleKeyConnectDialog',
    props: ['show', 'value', 'busy', 'pending', 'error'],
    emits: ['update:value', 'submit', 'close'],
    setup(props, { emit }) {
      const input = ref(null)
      const validKey = computed(() => Boolean(props.value.trim()) && props.value.trim().length <= 4096 && !/\s/.test(props.value.trim()))
      return () => h(NModal, {
        show: props.show, preset: 'card', title: `${labels.title} · ${labels.keyConnect}`,
        class: 'ts-key-dialog', maskClosable: false,
        closable: !props.busy, closeOnEsc: !props.busy,
        style: { width: 'min(440px, calc(100vw - 32px))', maxHeight: 'calc(100dvh - 32px)', overflow: 'auto' },
        onAfterEnter: () => input.value?.focus(),
        'onUpdate:show': show => { if (!show) emit('close') },
      }, { default: () => h('form', { class: 'ts-key-form', onSubmit: event => { event.preventDefault(); if (validKey.value && !props.busy && !props.pending) emit('submit') } }, [
        props.error ? h(NAlert, { type: 'error', showIcon: false, role: 'alert' }, { default: () => props.error }) : null,
        h('label', { for: 'tailscale-connection-key' }, labels.authKey),
        h(NInput, {
          ref: input, value: props.value, type: 'password', showPasswordOn: 'click', maxlength: 4096,
          placeholder: labels.pasteKey, disabled: props.busy || props.pending,
          inputProps: { id: 'tailscale-connection-key', 'aria-label': labels.authKey, autocomplete: 'off', spellcheck: false, autocapitalize: 'none' },
          'onUpdate:value': value => emit('update:value', value),
        }),
        h('div', { class: 'ts-key-actions' }, [
          h(NButton, { secondary: true, disabled: props.busy, onClick: () => emit('close') }, { default: () => labels.cancel }),
          h(NButton, { type: 'primary', attrType: 'submit', loading: props.busy || props.pending, disabled: !validKey.value || props.busy || props.pending }, { default: () => props.pending ? labels.connecting : labels.connect }),
        ]),
      ]) })
    },
  })

  const Page = defineComponent({
    name: 'TailscalePage',
    props: { host: { type: Object, required: true } },
    setup(props) {
      const initial = props.host.extension.value.settings
      const authKey = ref(String(initial.auth_key || ''))
      const controlURL = ref(String(initial.control_url || ''))
      const port = ref(Number(initial.port ?? 41641))
      const acceptRoutes = ref(initial.accept_routes === true)
      const shieldsUp = ref(initial.shields_up === true)
      const status = ref({ online: false, loggedIn: false, backendState: 'Offline', authURL: '', hostname: '', addresses: [] })
      const loading = ref(false)
      const awaitingAuthURL = ref(false)
      const keyOpen = ref(false)
      const keyDraft = ref('')
      const keyError = ref('')
      const keyPending = ref(false)
      let keyDeadline = 0
      const busy = ref('')
      const error = ref('')
      const saved = ref('')
      const message = useMessage()
      let timer = null
      let authDeadline = 0

      const snapshot = () => JSON.stringify([authKey.value, controlURL.value, port.value, acceptRoutes.value, shieldsUp.value])
      saved.value = snapshot()
      const dirty = computed(() => snapshot() !== saved.value)
      const valid = computed(() => authKey.value.length <= 4096 && controlURL.value.length <= 2048 && Number.isInteger(port.value) && port.value >= 0 && port.value <= 65535)
      const running = computed(() => status.value.backendState === 'Running')
      const loggedIn = computed(() => status.value.loggedIn === true)

      const unregister = props.host.registerSettings({
        dirty,
        valid,
        showHostActions: false,
        collect: () => ({ auth_key: authKey.value, control_url: controlURL.value, accept_routes: acceptRoutes.value, shields_up: shieldsUp.value, port: port.value }),
        reset: (settings) => {
          authKey.value = String(settings.auth_key || '')
          controlURL.value = String(settings.control_url || '')
          acceptRoutes.value = settings.accept_routes === true
          shieldsUp.value = settings.shields_up === true
          port.value = Number(settings.port ?? 41641)
          saved.value = snapshot()
        },
      })

      async function refresh(showSpinner = false) {
        if (loading.value) return
        loading.value = true
        if (showSpinner) busy.value = 'refresh'
        try {
          status.value = await props.host.invoke('status')
          error.value = ''
          if (keyPending.value && busy.value !== 'cancel-key') {
            if (running.value) {
              keyPending.value = false
              keyOpen.value = false
              keyDraft.value = ''
              keyError.value = ''
              message.success(labels.connected)
            } else if (['failed', 'expired', 'timed-out'].includes(status.value.authState)) {
              keyPending.value = false
              keyError.value = labels.connectionFailed
            } else if (Date.now() >= keyDeadline) {
              keyError.value = labels.connectionTimeout
            }
          }
          if (status.value.authURL || status.value.loggedIn) {
            awaitingAuthURL.value = false
            authDeadline = 0
          } else if (awaitingAuthURL.value && Date.now() >= authDeadline) {
            awaitingAuthURL.value = false
            authDeadline = 0
            error.value = labels.loginTimeout
          }
        } catch (reason) {
          error.value = reason instanceof Error ? reason.message : String(reason)
        } finally {
          loading.value = false
          if (busy.value === 'refresh') busy.value = ''
        }
      }

      async function action(name, success) {
        if (busy.value) return
        busy.value = name
        try {
          await props.host.invoke(name)
          message.success(success)
          await refresh()
        } catch (reason) {
          error.value = reason instanceof Error ? reason.message : String(reason)
        } finally {
          busy.value = ''
        }
      }

      async function connect() {
        if (busy.value || awaitingAuthURL.value) return
        busy.value = 'connect'
        awaitingAuthURL.value = true
        authDeadline = Date.now() + 60000
        const previousKey = authKey.value
        try {
          authKey.value = ''
          // Secret settings are omitted from API responses. Always send the
          // empty key when choosing browser sign-in, even if the form is clean.
          if (!await props.host.saveSettings()) {
            authKey.value = previousKey
            awaitingAuthURL.value = false
            authDeadline = 0
            return
          }
          await props.host.invoke('connect')
          message.success(labels.connectStarted)
          await refresh()
        } catch (reason) {
          awaitingAuthURL.value = false
          authDeadline = 0
          error.value = reason instanceof Error ? reason.message : String(reason)
        } finally {
          busy.value = ''
        }
      }

      function openKeyDialog() {
        if (busy.value || awaitingAuthURL.value) return
        keyDraft.value = ''
        keyError.value = ''
        keyOpen.value = true
      }

      async function closeKeyDialog() {
        if (busy.value) return
        if (keyPending.value) {
          busy.value = 'cancel-key'
          try {
            await props.host.invoke('disconnect')
            keyPending.value = false
            await refresh()
          } catch (reason) {
            keyError.value = reason instanceof Error ? reason.message : String(reason)
            return
          } finally {
            busy.value = ''
          }
        }
        keyOpen.value = false
        keyDraft.value = ''
        keyError.value = ''
      }

      async function connectWithKey() {
        const value = keyDraft.value.trim()
        if (busy.value || keyPending.value || !valid.value || !value || value.length > 4096 || /\s/.test(value)) return
        busy.value = 'key'
        keyError.value = ''
        const previousKey = authKey.value
        try {
          authKey.value = value
          if (dirty.value && !await props.host.saveSettings()) {
            authKey.value = previousKey
            keyError.value = labels.saveFailed
            return
          }
          await props.host.invoke('connect_key')
          keyPending.value = true
          keyDeadline = Date.now() + 60000
          await refresh()
        } catch (reason) {
          keyError.value = reason instanceof Error ? reason.message : String(reason)
        } finally {
          busy.value = ''
        }
      }

      async function copyURL() {
        if (!status.value.authURL) return
        try {
          await navigator.clipboard.writeText(status.value.authURL)
          message.success(labels.copied)
        } catch (reason) {
          error.value = reason instanceof Error ? reason.message : String(reason)
        }
      }

      async function saveSettings() {
        if (!valid.value || busy.value) return
        busy.value = 'save'
        try {
          if (await props.host.saveSettings()) {
            saved.value = snapshot()
            message.success(labels.saved)
            await refresh()
          }
        } finally {
          busy.value = ''
        }
      }

      onMounted(() => {
        void refresh(true)
        timer = window.setInterval(() => { void refresh(false) }, 3000)
      })
      onBeforeUnmount(() => {
        unregister()
        if (timer !== null) window.clearInterval(timer)
      })

      return () => h('section', { class: 'ts-page' }, [
        h('div', { class: 'ts-heading' }, [
          h('h2', labels.title),
          h(NButton, { secondary: true, loading: busy.value === 'refresh', disabled: Boolean(busy.value && busy.value !== 'refresh'), onClick: () => refresh(true) }, { default: () => labels.refresh }),
        ]),
        error.value ? h(NAlert, { type: 'error', showIcon: false }, { default: () => error.value }) : null,
        h(NSpin, { show: (loading.value && !status.value.online) || awaitingAuthURL.value }, { default: () => h('article', { class: 'ts-status' }, [
          h('div', { class: 'ts-status-title' }, [h('strong', labels.status), h(NTag, { type: running.value ? 'success' : status.value.online ? 'warning' : 'error', bordered: false }, { default: () => status.value.backendState || labels.offline })]),
          status.value.hostname ? h('span', status.value.hostname) : null,
          h('small', labels.ips),
          Array.isArray(status.value.addresses) && status.value.addresses.length
            ? h('div', { class: 'ts-addresses' }, status.value.addresses.map((ip) => h('code', { key: ip }, ip)))
            : h('span', { class: 'ts-muted' }, labels.noIP),
          status.value.authURL ? h('div', { class: 'ts-auth' }, [
            h('code', status.value.authURL),
            h('div', { class: 'ts-actions' }, [
              h('a', { href: status.value.authURL, target: '_blank', rel: 'noopener noreferrer' }, [h(NButton, { type: 'primary' }, { default: () => labels.openLogin })]),
              h(NButton, { secondary: true, onClick: copyURL }, { default: () => labels.copyURL }),
            ]),
          ]) : null,
          h('div', { class: 'ts-actions' }, [
            !running.value && !status.value.authURL ? h(NButton, { type: 'primary', loading: busy.value === 'connect' || awaitingAuthURL.value, disabled: !status.value.online || !valid.value || keyPending.value || awaitingAuthURL.value || Boolean(busy.value && busy.value !== 'connect'), onClick: connect }, { default: () => awaitingAuthURL.value ? labels.starting : labels.browserConnect }) : null,
            !running.value && !status.value.authURL ? h(NButton, { secondary: true, disabled: !status.value.online || !valid.value || awaitingAuthURL.value || keyPending.value || Boolean(busy.value) || ['starting', 'pending', 'connecting'].includes(status.value.authState), onClick: openKeyDialog }, { default: () => labels.keyConnect }) : null,
            running.value ? h(NButton, { secondary: true, loading: busy.value === 'disconnect', disabled: Boolean(busy.value && busy.value !== 'disconnect'), onClick: () => action('disconnect', labels.disconnected) }, { default: () => labels.disconnect }) : null,
            loggedIn.value ? h(NButton, { type: 'error', secondary: true, loading: busy.value === 'logout', disabled: Boolean(busy.value && busy.value !== 'logout'), onClick: () => action('logout', labels.loggedOut) }, { default: () => labels.logout }) : null,
          ]),
        ]) }),
        h('article', { class: 'ts-settings' }, [
          h('strong', labels.settings),
          h('div', { class: 'ts-grid' }, [
            h('label', [h('span', labels.controlURL), h(NInput, { value: controlURL.value, maxlength: 2048, disabled: keyOpen.value || Boolean(busy.value), 'onUpdate:value': (value) => { controlURL.value = value } })]),
            h('label', [h('span', labels.port), h(NInputNumber, { value: port.value, min: 0, max: 65535, disabled: keyOpen.value || Boolean(busy.value), 'onUpdate:value': (value) => { port.value = value ?? 0 } })]),
            h('label', { class: 'ts-switch' }, [h('span', labels.routes), h(NSwitch, { value: acceptRoutes.value, disabled: keyOpen.value || Boolean(busy.value), 'onUpdate:value': (value) => { acceptRoutes.value = value } })]),
            h('label', { class: 'ts-switch' }, [h('span', labels.shields), h(NSwitch, { value: shieldsUp.value, disabled: keyOpen.value || Boolean(busy.value), 'onUpdate:value': (value) => { shieldsUp.value = value } })]),
          ]),
          h('div', { class: 'ts-save' }, [h(NButton, { type: 'primary', loading: busy.value === 'save', disabled: keyOpen.value || !dirty.value || !valid.value || Boolean(busy.value && busy.value !== 'save'), onClick: saveSettings }, { default: () => labels.save })]),
        ]),
        h(KeyConnectDialog, {
          show: keyOpen.value, value: keyDraft.value, error: keyError.value,
          busy: Boolean(busy.value), pending: keyPending.value,
          'onUpdate:value': value => { keyDraft.value = value; keyError.value = '' },
          onSubmit: connectWithKey, onClose: closeKeyDialog,
        }),
      ])
    },
  })

  runtime.register('tailscale', {
    apiVersion: 1,
    component: Page,
    styles: [`
      .ts-key-form { display: grid; gap: 12px; }
      .ts-key-actions { display: flex; flex-wrap: wrap; justify-content: flex-end; gap: 8px; margin-top: 4px; }
      .ts-page { display: grid; gap: 14px; }
      .ts-heading { display: flex; align-items: flex-start; justify-content: space-between; gap: 12px; }
      .ts-heading h2 { margin: 0; font-size: 18px; }
      .ts-status, .ts-settings { display: grid; gap: 12px; padding: 14px; border: 1px solid var(--n-border-color, #444); border-radius: 8px; }
      .ts-status-title { display: flex; align-items: center; justify-content: space-between; gap: 12px; }
      .ts-status small, .ts-settings small, .ts-muted { color: var(--n-text-color-3, #888); }
      .ts-addresses, .ts-actions { display: flex; flex-wrap: wrap; align-items: center; gap: 8px; }
      .ts-addresses code, .ts-auth > code { color: var(--n-text-color-1, #e5e7eb); font-family: var(--onekvm-font-mono, ui-monospace, "SFMono-Regular", Menlo, Monaco, Consolas, "Liberation Mono", monospace); font-size: 13px; font-feature-settings: "tnum" 1, "zero" 1; font-variant-numeric: tabular-nums slashed-zero; font-weight: 500; line-height: 1.5; letter-spacing: .01em; }
      .ts-auth { display: grid; gap: 9px; padding: 12px; border-radius: 7px; background: var(--n-color-modal, rgba(127,127,127,.08)); }
      .ts-auth code { overflow-wrap: anywhere; }
      .ts-auth a { text-decoration: none; }
      .ts-settings > label, .ts-grid > label { display: grid; gap: 7px; }
      .ts-grid { display: grid; grid-template-columns: minmax(0, 1fr); gap: 13px; }
      .ts-grid .ts-switch { display: grid; gap: 7px; }
      .ts-grid .n-switch { justify-self: start; }
      .ts-save { display: flex; justify-content: flex-end; }
    `],
  })
})()
