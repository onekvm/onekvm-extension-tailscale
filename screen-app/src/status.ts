import type { Status, Translator } from "@onekvm/screen-ui";

function text(value: unknown, fallback: string): string {
  return typeof value === "string" && value ? value : fallback;
}

export function statusLabel(state: Status, t: Translator): string {
  return t(state.loggedIn === true ? "connected" : "disconnected");
}

export function summary(state: Status, t: Translator): string {
  return state.loggedIn === true ? text(Array.isArray(state.addresses) ? state.addresses[0] : "", t("connected")) : t("disconnected");
}
