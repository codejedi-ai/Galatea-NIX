import ReactDOM from "react-dom/client";
import App from "./App.jsx";

// Screen-only: the app is a single full-window terminal (ttyd-style). The other
// pages (CPU sim, etc.) are kept in the tree but not mounted for now.
ReactDOM.createRoot(document.getElementById("root")).render(<App />);
