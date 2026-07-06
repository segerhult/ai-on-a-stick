const machineStatusEl = document.getElementById("machine-status");
const runtimeStatusEl = document.getElementById("runtime-status");
const filesEl = document.getElementById("files");
const messagesEl = document.getElementById("messages");
const formEl = document.getElementById("chat-form");
const promptEl = document.getElementById("prompt");
const chatModeEl = document.getElementById("chat-mode");
const newChatEl = document.getElementById("new-chat");
const conversationEl = document.querySelector(".conversation");
const SCROLL_BOTTOM_THRESHOLD = 96;

let streamingInFlight = false;

function escapeHtml(value) {
  return value
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

function renderInlineMarkdown(text) {
  let html = escapeHtml(text);
  html = html.replace(/\[([^\]]+)\]\((https?:\/\/[^\s)]+)\)/g, '<a href="$2" target="_blank" rel="noreferrer">$1</a>');
  html = html.replace(/`([^`]+)`/g, "<code>$1</code>");
  html = html.replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>");
  html = html.replace(/\*([^*]+)\*/g, "<em>$1</em>");
  return html;
}

function renderMarkdownBlocks(markdown) {
  const lines = markdown.split("\n");
  const blocks = [];
  let paragraph = [];
  let listItems = [];
  let orderedItems = [];

  const flushParagraph = () => {
    if (paragraph.length === 0) {
      return;
    }
    const content = paragraph.map((line) => renderInlineMarkdown(line)).join("<br />");
    blocks.push(`<p>${content}</p>`);
    paragraph = [];
  };

  const flushList = () => {
    if (listItems.length === 0) {
      return;
    }
    blocks.push(`<ul>${listItems.map((item) => `<li>${renderInlineMarkdown(item)}</li>`).join("")}</ul>`);
    listItems = [];
  };

  const flushOrderedList = () => {
    if (orderedItems.length === 0) {
      return;
    }
    blocks.push(`<ol>${orderedItems.map((item) => `<li>${renderInlineMarkdown(item)}</li>`).join("")}</ol>`);
    orderedItems = [];
  };

  for (const line of lines) {
    const trimmed = line.trim();
    if (!trimmed) {
      flushParagraph();
      flushList();
      flushOrderedList();
      continue;
    }

    const headingMatch = trimmed.match(/^(#{1,3})\s+(.*)$/);
    if (headingMatch) {
      flushParagraph();
      flushList();
      flushOrderedList();
      const level = Math.min(headingMatch[1].length + 2, 6);
      blocks.push(`<h${level}>${renderInlineMarkdown(headingMatch[2])}</h${level}>`);
      continue;
    }

    const quoteMatch = trimmed.match(/^>\s?(.*)$/);
    if (quoteMatch) {
      flushParagraph();
      flushList();
      flushOrderedList();
      blocks.push(`<blockquote>${renderInlineMarkdown(quoteMatch[1])}</blockquote>`);
      continue;
    }

    const listMatch = trimmed.match(/^[-*]\s+(.*)$/);
    if (listMatch) {
      flushParagraph();
      flushOrderedList();
      listItems.push(listMatch[1]);
      continue;
    }

    const orderedMatch = trimmed.match(/^\d+\.\s+(.*)$/);
    if (orderedMatch) {
      flushParagraph();
      flushList();
      orderedItems.push(orderedMatch[1]);
      continue;
    }

    flushList();
    flushOrderedList();
    paragraph.push(trimmed);
  }

  flushParagraph();
  flushList();
  flushOrderedList();
  return blocks.join("");
}

function renderMarkdown(markdown) {
  const codeFence = /```([\w+-]*)\n?([\s\S]*?)```/g;
  let result = "";
  let lastIndex = 0;
  let match;

  while ((match = codeFence.exec(markdown)) !== null) {
    const [fullMatch, language, code] = match;
    result += renderMarkdownBlocks(markdown.slice(lastIndex, match.index));
    result += `<pre><code class="language-${escapeHtml(language || "text")}">${escapeHtml(code.trimEnd())}</code></pre>`;
    lastIndex = match.index + fullMatch.length;
  }

  result += renderMarkdownBlocks(markdown.slice(lastIndex));
  return result || "<p></p>";
}

function isNearConversationBottom() {
  const remaining = conversationEl.scrollHeight - conversationEl.scrollTop - conversationEl.clientHeight;
  return remaining <= SCROLL_BOTTOM_THRESHOLD;
}

function autosizePrompt() {
  promptEl.style.height = "0px";
  promptEl.style.height = `${Math.min(promptEl.scrollHeight, 220)}px`;
}

function scrollConversation(force = false) {
  if (!force && !isNearConversationBottom()) {
    return;
  }
  conversationEl.scrollTop = conversationEl.scrollHeight;
}

function createMessage(role) {
  const wrapper = document.createElement("article");
  wrapper.className = `message ${role}`;

  const body = document.createElement("div");
  body.className = "message-body";

  const meta = document.createElement("div");
  meta.className = "message-meta";

  wrapper.appendChild(body);
  wrapper.appendChild(meta);
  messagesEl.appendChild(wrapper);
  scrollConversation(true);
  return { wrapper, body, meta };
}

function setMessageContent(message, role, content, forceScroll = false) {
  if (role === "assistant") {
    message.body.innerHTML = renderMarkdown(content);
  } else {
    message.body.textContent = content;
  }
  scrollConversation(forceScroll);
}

function setMessageMeta(message, text) {
  message.meta.textContent = text || "";
  message.meta.style.display = text ? "block" : "none";
}

function formatTokPerSecond(timings) {
  const value = timings?.predicted_per_second;
  if (typeof value !== "number" || Number.isNaN(value)) {
    return "";
  }
  return `${value.toFixed(1)} tok/s`;
}

function formatDuration(ms) {
  if (typeof ms !== "number" || !Number.isFinite(ms) || ms <= 0) {
    return "";
  }
  if (ms < 1000) {
    return `${Math.round(ms)} ms`;
  }
  return `${(ms / 1000).toFixed(1)} s`;
}

function formatMemoryGb(value) {
  if (typeof value !== "number" || !Number.isFinite(value) || value < 0) {
    return "";
  }
  return `${value.toFixed(1)} GB`;
}

function describeRuntimeState(runtime) {
  if (!runtime.ready) {
    return "needs bundle";
  }
  if (runtime.healthy) {
    const loadedIn = formatDuration(runtime.ready_elapsed_ms);
    return loadedIn ? `live · loaded in ${loadedIn}` : "live";
  }
  if (runtime.running) {
    const elapsed = formatDuration(runtime.startup_elapsed_ms);
    return elapsed ? `loading · ${elapsed}` : "loading";
  }
  if (runtime.last_exit_status) {
    return `failed · exit ${runtime.last_exit_status}`;
  }
  return runtime.state || "stopped";
}

function formatAssistantMeta({ mode, timings, done }) {
  const modeLabel = mode === "coder" ? "Coder" : "General";
  const speed = formatTokPerSecond(timings);
  if (done && speed) {
    return `${modeLabel} · ${speed}`;
  }
  if (done) {
    return `${modeLabel} · Complete`;
  }
  if (speed) {
    return `${modeLabel} · ${speed}`;
  }
  return `${modeLabel} · Streaming...`;
}

function buildPayload(prompt, mode) {
  return {
    model: "local-usb-model",
    mode,
    stream: true,
    messages: [
      {
        role: "system",
        content: "You are the portable local assistant running from AI-On-A-Stick. Respond in markdown when useful."
      },
      {
        role: "user",
        content: prompt
      }
    ]
  };
}

async function readJsonError(response) {
  const text = await response.text();
  try {
    return JSON.parse(text).error || text;
  } catch {
    return text || `Request failed with ${response.status}`;
  }
}

async function loadStatus() {
  const response = await fetch("/api/status");
  const status = await response.json();
  const memoryUsed = formatMemoryGb(status.memory_used_gb);
  const memoryAvailable = formatMemoryGb(status.memory_available_gb);
  const memoryTotal = formatMemoryGb(status.memory_gb);
  const hostRamSummary = memoryUsed && memoryTotal
    ? `${memoryUsed} used / ${memoryTotal} total`
    : memoryTotal || "unknown";
  const hostRamDetail = memoryUsed && memoryAvailable
    ? `${memoryUsed} used / ${memoryAvailable} free`
    : hostRamSummary;
  machineStatusEl.innerHTML = `
    <div><strong>Machine</strong><br />${status.hostname}</div>
    <div><strong>OS / Arch</strong><br />${status.operating_system} / ${status.architecture}</div>
    <div><strong>Hardware</strong><br />${status.logical_cores} cores / ${status.memory_gb} GB / ${status.gpu_backend}</div>
    <div><strong>RAM</strong><br />${hostRamDetail}</div>
    <div><strong>Profile</strong><br />${status.runtime_profile}</div>
    <div><strong>Indexed</strong><br />${status.indexed_files} files</div>
  `;

  runtimeStatusEl.innerHTML = "";
  for (const runtime of Object.values(status.runtimes ?? {})) {
    const card = document.createElement("div");
    card.className = "runtime-card";
    const loadMeta = runtime.healthy
      ? formatDuration(runtime.ready_elapsed_ms)
      : formatDuration(runtime.startup_elapsed_ms);
    const runtimeRam = runtime.healthy || runtime.running ? hostRamSummary : "idle";
    card.innerHTML = `
      <h3>${runtime.name}</h3>
      <div class="runtime-meta">${describeRuntimeState(runtime)}</div>
      <div><strong>Endpoint</strong><br />${runtime.base_url}</div>
      <div><strong>Load</strong><br />${loadMeta || "waiting"} / port ${runtime.port}</div>
      <div><strong>Status</strong><br />${runtime.healthy ? "model loaded" : runtime.running ? "loading model" : runtime.last_exit_status ? `load failed (${runtime.last_exit_status})` : "not active"}</div>
      <div><strong>RAM</strong><br />${runtimeRam}</div>
      <div><strong>Speed</strong><br />${runtime.parallel_slots} slots / ${runtime.gpu_layers} layers</div>
    `;
    runtimeStatusEl.appendChild(card);
  }
}

async function loadFiles() {
  const response = await fetch("/api/files");
  const files = await response.json();
  filesEl.innerHTML = "";

  for (const file of files) {
    const li = document.createElement("li");
    li.innerHTML = `
      <div>${file.path}</div>
      <div class="file-meta">${file.size_bytes} bytes · ${file.machine_id}</div>
    `;
    filesEl.appendChild(li);
  }
}

function consumeSseEvent(rawEvent, onData) {
  for (const rawLine of rawEvent.split("\n")) {
    const line = rawLine.trim();
    if (!line.startsWith("data:")) {
      continue;
    }

    const data = line.slice(5).trim();
    if (!data) {
      continue;
    }
    onData(data);
  }
}

async function streamChat(prompt, mode) {
  const assistantMessage = createMessage("assistant");
  setMessageContent(assistantMessage, "assistant", "", true);
  setMessageMeta(assistantMessage, formatAssistantMeta({ mode, done: false }));

  const response = await fetch("/api/chat/stream", {
    method: "POST",
    headers: {
      "Content-Type": "application/json"
    },
    body: JSON.stringify(buildPayload(prompt, mode))
  });

  if (!response.ok || !response.body) {
    throw new Error(await readJsonError(response));
  }

  const reader = response.body.getReader();
  const decoder = new TextDecoder();
  let buffer = "";
  let content = "";
  let timings = null;
  let renderScheduled = false;
  let stickToBottom = true;

  const flushRender = (done = false) => {
    renderScheduled = false;
    setMessageContent(assistantMessage, "assistant", content || (done ? "_No response from model._" : ""), stickToBottom);
    setMessageMeta(assistantMessage, formatAssistantMeta({ mode, timings, done }));
  };

  const scheduleRender = () => {
    if (renderScheduled) {
      return;
    }
    renderScheduled = true;
    requestAnimationFrame(() => flushRender(false));
  };

  while (true) {
    const { value, done } = await reader.read();
    if (done) {
      break;
    }

    stickToBottom = isNearConversationBottom();
    buffer += decoder.decode(value, { stream: true });
    let separatorIndex = buffer.indexOf("\n\n");
    while (separatorIndex !== -1) {
      const rawEvent = buffer.slice(0, separatorIndex);
      buffer = buffer.slice(separatorIndex + 2);

      consumeSseEvent(rawEvent, (data) => {
        if (data === "[DONE]") {
          return;
        }

        let json;
        try {
          json = JSON.parse(data);
        } catch {
          return;
        }

        const delta = json.choices?.[0]?.delta?.content ?? "";
        if (delta) {
          content += delta;
          scheduleRender();
        }

        if (json.timings) {
          timings = json.timings;
          scheduleRender();
        }
      });

      separatorIndex = buffer.indexOf("\n\n");
    }
  }

  if (buffer.trim()) {
    consumeSseEvent(buffer, (data) => {
      if (data === "[DONE]") {
        return;
      }
      try {
        const json = JSON.parse(data);
        const delta = json.choices?.[0]?.delta?.content ?? "";
        if (delta) {
          content += delta;
        }
        if (json.timings) {
          timings = json.timings;
        }
      } catch {
        // Ignore any incomplete trailing event fragments.
      }
    });
  }

  flushRender(true);
}

async function runAgent(prompt, mode) {
  const assistantMessage = createMessage("assistant");
  setMessageMeta(assistantMessage, "Agent · Working...");

  const response = await fetch("/api/agent", {
    method: "POST",
    headers: {
      "Content-Type": "application/json"
    },
    body: JSON.stringify({ prompt, mode })
  });

  const json = await response.json();
  const content = json.reply ?? json.error ?? "No response from model.";
  setMessageContent(assistantMessage, "assistant", content, true);
  setMessageMeta(assistantMessage, Array.isArray(json.actions) && json.actions.length > 0 ? json.actions.join(" · ") : "");
}

formEl.addEventListener("submit", async (event) => {
  event.preventDefault();
  if (streamingInFlight) {
    return;
  }
  const prompt = promptEl.value.trim();
  if (!prompt) {
    return;
  }

  const mode = chatModeEl.value;
  const userMessage = createMessage("user");
  setMessageContent(userMessage, "user", prompt);
  setMessageMeta(userMessage, "");

  promptEl.value = "";
  autosizePrompt();
  promptEl.disabled = true;
  formEl.querySelector(".send-button").disabled = true;
  streamingInFlight = true;

  try {
    if (mode === "agent") {
      await runAgent(prompt, mode);
    } else {
      await streamChat(prompt, mode);
    }
  } catch (error) {
    const assistantMessage = createMessage("assistant");
    setMessageContent(assistantMessage, "assistant", `Error: ${error.message}`, true);
    setMessageMeta(assistantMessage, "Request failed");
  } finally {
    promptEl.disabled = false;
    formEl.querySelector(".send-button").disabled = false;
    streamingInFlight = false;
    promptEl.focus();
  }
});

newChatEl.addEventListener("click", () => {
  messagesEl.innerHTML = "";
  promptEl.focus();
});

promptEl.addEventListener("input", autosizePrompt);
promptEl.addEventListener("keydown", (event) => {
  if (event.key === "Enter" && !event.shiftKey) {
    event.preventDefault();
    formEl.requestSubmit();
  }
});

autosizePrompt();
loadStatus();
loadFiles();
window.setInterval(() => {
  loadStatus().catch(() => {});
}, 1000);
