import { render } from "preact";
import { App } from "./app";
import { ToastProvider } from "./components/Toast";

const root = document.getElementById("app");
if (root) {
  render(
    <ToastProvider>
      <App />
    </ToastProvider>,
    root,
  );
}
