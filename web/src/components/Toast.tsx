import { createContext } from "preact";
import type { ComponentChildren } from "preact";
import { useContext, useState, useCallback } from "preact/hooks";

interface ToastItem {
  id: number;
  msg: string;
  error?: boolean;
}
type Notify = (msg: string, error?: boolean) => void;

const ToastCtx = createContext<Notify>(() => {});
export const useToast = (): Notify => useContext(ToastCtx);

export function ToastProvider({ children }: { children: ComponentChildren }) {
  const [toasts, setToasts] = useState<ToastItem[]>([]);

  const notify = useCallback<Notify>((msg, error) => {
    const id = Date.now() + Math.random();
    setToasts((t) => [...t, { id, msg, error }]);
    setTimeout(() => setToasts((t) => t.filter((x) => x.id !== id)), 3500);
  }, []);

  return (
    <ToastCtx.Provider value={notify}>
      {children}
      <div class="toasts">
        {toasts.map((t) => (
          <div key={t.id} class={t.error ? "toast error" : "toast"}>
            {t.msg}
          </div>
        ))}
      </div>
    </ToastCtx.Provider>
  );
}
