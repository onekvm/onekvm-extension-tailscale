import type { Action, Authorization, MessageKey, Status, Translator } from "@onekvm/screen-ui";

function authorizationActive(state: Status): boolean {
  return ["starting", "pending", "connecting"].includes(String(state.authState)) || state.backendState === "Starting";
}

export function actionLabel(state: Status, action: Action): MessageKey | undefined {
  return action.method === "disconnect" && state.backendState !== "Running" && authorizationActive(state) ? "cancel" : undefined;
}

export function actionEnabled(state: Status, action: Action): boolean {
  const active = authorizationActive(state);
  const connected = state.backendState === "Running";
  if (action.method === "connect") return state.online === true && !active && !connected;
  if (action.method === "disconnect") return state.online === true && (active || connected);
  return true;
}

function text(value: unknown, fallback: string): string {
  return typeof value === "string" && value ? value : fallback;
}

export function statusLabel(state: Status, t: Translator): string {
  return t(state.backendState === "Running" ? "connected" : "disconnected");
}

export function summary(state: Status, t: Translator): string {
  return state.loggedIn === true ? text(Array.isArray(state.addresses) ? state.addresses[0] : "", t("connected")) : t("disconnected");
}

export function authorization(state: Status): Authorization {
  if (state.loggedIn === true) return { state: "idle" };
  const phase = state.authState;
  if (phase === "starting" || phase === "connecting" || phase === "expired" || phase === "failed" || phase === "timed-out") return { state: phase };
  return state.authURL && phase === "pending" ? { state: "pending", url: String(state.authURL) } : { state: "idle" };
}
