"use strict";

    const DOWNLOAD_MAX_SIZE = 0x20000;
    const FRAME_START = 0x01;
    const FRAME_DATA = 0x02;
    const FRAME_END = 0x03;
    const FRAME_ACK = 0x79;
    const COMPAT_SAFE_WRITE_SIZE = 17;
    const COMPAT_MIN_WRITE_SIZE = 14;
    const COMPAT_MIN_PAYLOAD_SIZE = 16;
    const COMPAT_TAIL_PAD_SIZE = 8;
    const COMPAT_TAIL_PAD_GUARD_WRITES = 1;
    const FRAME_TAIL_PAD = 0x55;
    const RX_TIMEOUT_STATUS = 9;
    const ACK_STATUS_NAMES = {
      0: "OK",
      1: "BAD_MAGIC",
      2: "BAD_SIZE",
      3: "BAD_SEQUENCE",
      4: "BAD_OFFSET",
      5: "BAD_PACKET_CRC",
      6: "FLASH_FAILED",
      7: "IMAGE_CRC_FAILED",
      8: "VERSION_REJECTED",
      9: "RX_TIMEOUT"
    };
    const DEFAULT_SERVICE_UUID = "0000fff0-0000-1000-8000-00805f9b34fb";
    const DEFAULT_WRITE_UUID = "0000fff3-0000-1000-8000-00805f9b34fb";
    const DEFAULT_NOTIFY_UUID = "0000fff1-0000-1000-8000-00805f9b34fb";
    const UUID_HISTORY_LIMIT = 8;
    const STORAGE_KEYS = {
      serviceUuids: "webBleOta.serviceUuidHistory",
      writeUuids: "webBleOta.writeUuidHistory",
      notifyUuids: "webBleOta.notifyUuidHistory"
    };

    const els = {
      file: document.getElementById("fileInput"),
      version: document.getElementById("versionInput"),
      payloadSize: document.getElementById("payloadSizeInput"),
      ackTimeout: document.getElementById("ackTimeoutInput"),
      retry: document.getElementById("retryInput"),
      ackDelay: document.getElementById("ackDelayInput"),
      writeChunk: document.getElementById("writeChunkInput"),
      chunkDelay: document.getElementById("chunkDelayInput"),
      compatMode: document.getElementById("compatModeInput"),
      serviceUuid: document.getElementById("serviceUuidInput"),
      writeUuid: document.getElementById("writeUuidInput"),
      notifyUuid: document.getElementById("notifyUuidInput"),
      namePrefix: document.getElementById("namePrefixInput"),
      writeMode: document.getElementById("writeModeInput"),
      connect: document.getElementById("connectButton"),
      start: document.getElementById("startButton"),
      stop: document.getElementById("stopButton"),
      inspectNotify: document.getElementById("inspectNotifyButton"),
      disconnect: document.getElementById("disconnectButton"),
      clearLog: document.getElementById("clearLogButton"),
      copyLog: document.getElementById("copyLogButton"),
      status: document.getElementById("connectionStatus"),
      log: document.getElementById("log"),
      sizeMetric: document.getElementById("sizeMetric"),
      crcMetric: document.getElementById("crcMetric"),
      sentMetric: document.getElementById("sentMetric"),
      speedMetric: document.getElementById("speedMetric"),
      progress: document.getElementById("progressBar"),
      serviceUuidHistory: document.getElementById("serviceUuidHistory"),
      writeUuidHistory: document.getElementById("writeUuidHistory"),
      notifyUuidHistory: document.getElementById("notifyUuidHistory")
    };

    const state = {
      device: null,
      server: null,
      writeChar: null,
      notifyChar: null,
      fileBytes: null,
      imageCrc: null,
      otaRunning: false,
      stopRequested: false,
      inspectNextNotify: false,
      pendingAck: null,
      ackCache: [],
      ackByteBuffer: [],
      ackTextBuffer: "",
      lastNonAckNotifies: [],
      lastNotifyHex: "",
      notifyCount: 0,
      ackCount: 0,
      retryCount: 0,
      startedAt: 0,
      tx: {
        chunkSize: COMPAT_SAFE_WRITE_SIZE,
        chunkDelayMs: 20,
        ackDelayMs: 150,
        tailPadSize: COMPAT_TAIL_PAD_SIZE
      }
    };

    const crcTable = (() => {
      const table = new Uint32Array(256);
      for (let i = 0; i < 256; i++) {
        let c = i;
        for (let bit = 0; bit < 8; bit++) {
          c = (c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1);
        }
        table[i] = c >>> 0;
      }
      return table;
    })();

    function crc32(bytes) {
      let crc = 0xFFFFFFFF;
      for (let i = 0; i < bytes.length; i++) {
        crc = crcTable[(crc ^ bytes[i]) & 0xFF] ^ (crc >>> 8);
      }
      return (~crc) >>> 0;
    }

    function hex32(value) {
      return "0x" + (value >>> 0).toString(16).toUpperCase().padStart(8, "0");
    }

    function ackStatusName(status) {
      return ACK_STATUS_NAMES[status] || `STATUS_${status}`;
    }

    function describeResetCsr(csr) {
      const flags = [];
      if (csr & 0x02000000) flags.push("BORRST");
      if (csr & 0x04000000) flags.push("PINRST");
      if (csr & 0x08000000) flags.push("PORRST");
      if (csr & 0x10000000) flags.push("SFTRST");
      if (csr & 0x20000000) flags.push("IWDGRST");
      if (csr & 0x40000000) flags.push("WWDGRST");
      if (csr & 0x80000000) flags.push("LPWRRST");
      return `${hex32(csr)}${flags.length ? " " + flags.join("|") : ""}`;
    }

    function log(message) {
      const time = new Date().toLocaleTimeString();
      els.log.textContent += `[${time}] ${message}\n`;
      if (els.log.textContent.length > 120000) {
        els.log.textContent = els.log.textContent.slice(-90000);
      }
      els.log.scrollTop = els.log.scrollHeight;
    }

    async function copyLogText() {
      const text = els.log.textContent;
      if (!text.trim()) {
        log("日志为空，无需复制");
        return;
      }

      if (navigator.clipboard && window.isSecureContext) {
        await navigator.clipboard.writeText(text);
        log("日志已复制到剪贴板");
        return;
      }

      const textarea = document.createElement("textarea");
      textarea.value = text;
      textarea.setAttribute("readonly", "");
      textarea.style.position = "fixed";
      textarea.style.left = "-9999px";
      textarea.style.top = "0";
      document.body.appendChild(textarea);
      textarea.select();

      try {
        if (!document.execCommand("copy")) {
          throw new Error("execCommand copy 返回失败");
        }
        log("日志已复制到剪贴板");
      } finally {
        document.body.removeChild(textarea);
      }
    }

    function setStatus(text, kind) {
      els.status.classList.toggle("connected", kind === "connected");
      els.status.classList.toggle("error", kind === "error");
      els.status.querySelector("span:last-child").textContent = text;
    }

    function readUintInput(input, fallback) {
      const value = Number(input.value);
      if (!Number.isFinite(value)) {
        return fallback;
      }
      return Math.trunc(value);
    }

    function sleep(ms) {
      return new Promise((resolve) => window.setTimeout(resolve, ms));
    }

    function throwIfStopRequested() {
      if (state.stopRequested) {
        throw new Error("用户已停止 OTA");
      }
    }

    function normalizeUuid(value) {
      return value.trim().toLowerCase();
    }

    function readHistory(key, fallback) {
      try {
        const parsed = JSON.parse(localStorage.getItem(key) || "[]");
        if (Array.isArray(parsed)) {
          return parsed.filter((item) => typeof item === "string" && item.trim());
        }
      } catch (error) {
        log(`读取 UUID 历史失败：${error.message}`);
      }
      return fallback.slice();
    }

    function writeHistory(key, values) {
      try {
        localStorage.setItem(key, JSON.stringify(values.slice(0, UUID_HISTORY_LIMIT)));
      } catch (error) {
        log(`保存 UUID 历史失败：${error.message}`);
      }
    }

    function renderUuidHistory(datalist, values) {
      datalist.textContent = "";
      for (const value of values) {
        const option = document.createElement("option");
        option.value = value;
        datalist.appendChild(option);
      }
    }

    function addHistoryValue(key, datalist, value, fallback) {
      const normalized = normalizeUuid(value);
      if (!normalized) {
        return;
      }

      const current = readHistory(key, fallback).map(normalizeUuid);
      const next = [normalized, ...current.filter((item) => item !== normalized)]
        .slice(0, UUID_HISTORY_LIMIT);
      writeHistory(key, next);
      renderUuidHistory(datalist, next);
    }

    function loadUuidHistories() {
      const serviceHistory = readHistory(STORAGE_KEYS.serviceUuids, [DEFAULT_SERVICE_UUID])
        .map(normalizeUuid);
      const writeHistoryValues = readHistory(STORAGE_KEYS.writeUuids, [DEFAULT_WRITE_UUID])
        .map(normalizeUuid);
      const notifyHistoryValues = readHistory(STORAGE_KEYS.notifyUuids, [DEFAULT_NOTIFY_UUID])
        .map(normalizeUuid);
      const serviceValues = [DEFAULT_SERVICE_UUID, ...serviceHistory.filter((item) => item !== DEFAULT_SERVICE_UUID)];
      const writeValues = [DEFAULT_WRITE_UUID, ...writeHistoryValues.filter((item) => item !== DEFAULT_WRITE_UUID)];
      const notifyValues = [DEFAULT_NOTIFY_UUID, ...notifyHistoryValues.filter((item) => item !== DEFAULT_NOTIFY_UUID)];

      renderUuidHistory(els.serviceUuidHistory, serviceValues);
      renderUuidHistory(els.writeUuidHistory, writeValues);
      renderUuidHistory(els.notifyUuidHistory, notifyValues);

      if (serviceValues[0]) {
        els.serviceUuid.value = serviceValues[0];
      }
      if (writeValues[0]) {
        els.writeUuid.value = writeValues[0];
      }
      if (notifyValues[0]) {
        els.notifyUuid.value = notifyValues[0];
      }
    }

    function saveCurrentUuidHistories() {
      addHistoryValue(
        STORAGE_KEYS.serviceUuids,
        els.serviceUuidHistory,
        els.serviceUuid.value,
        [DEFAULT_SERVICE_UUID]
      );
      addHistoryValue(
        STORAGE_KEYS.writeUuids,
        els.writeUuidHistory,
        els.writeUuid.value,
        [DEFAULT_WRITE_UUID]
      );
      addHistoryValue(
        STORAGE_KEYS.notifyUuids,
        els.notifyUuidHistory,
        els.notifyUuid.value,
        [DEFAULT_NOTIFY_UUID]
      );
    }

    function setControls() {
      const connected = Boolean(state.writeChar && state.notifyChar);
      els.connect.disabled = connected || state.otaRunning;
      els.start.disabled = !connected || !state.fileBytes || state.otaRunning;
      els.stop.disabled = !state.otaRunning || state.stopRequested;
      els.inspectNotify.disabled = !connected || state.otaRunning || state.inspectNextNotify;
      els.disconnect.disabled = !connected || state.otaRunning;
      els.file.disabled = state.otaRunning;
    }

    function describeProperties(characteristic) {
      const props = characteristic.properties;
      return [
        props.read ? "read" : null,
        props.write ? "write" : null,
        props.writeWithoutResponse ? "writeWithoutResponse" : null,
        props.notify ? "notify" : null,
        props.indicate ? "indicate" : null
      ].filter(Boolean).join(", ") || "none";
    }

    function updateProgress(sentBytes) {
      const total = state.fileBytes ? state.fileBytes.length : 0;
      const ratio = total > 0 ? sentBytes / total : 0;
      const percent = Math.min(100, Math.max(0, ratio * 100));
      els.progress.style.width = percent.toFixed(2) + "%";
      els.sentMetric.textContent = percent.toFixed(1) + "%";

      if (state.startedAt && sentBytes > 0) {
        const seconds = (performance.now() - state.startedAt) / 1000;
        const speed = sentBytes / Math.max(seconds, 0.001);
        els.speedMetric.textContent = `${Math.round(speed)} B/s`;
      }
    }

    async function loadSelectedFile() {
      const file = els.file.files[0];
      state.fileBytes = null;
      state.imageCrc = null;
      updateProgress(0);
      els.speedMetric.textContent = "-";

      if (!file) {
        els.sizeMetric.textContent = "-";
        els.crcMetric.textContent = "-";
        setControls();
        return;
      }

      const bytes = new Uint8Array(await file.arrayBuffer());
      if (bytes.length === 0) {
        throw new Error("app.bin 为空");
      }
      if (bytes.length > DOWNLOAD_MAX_SIZE) {
        throw new Error(`app.bin 超过下载区大小：${bytes.length} > ${DOWNLOAD_MAX_SIZE}`);
      }

      state.fileBytes = bytes;
      state.imageCrc = crc32(bytes);
      els.sizeMetric.textContent = `${bytes.length} B`;
      els.crcMetric.textContent = hex32(state.imageCrc);
      log(`已载入 ${file.name}，size=${bytes.length}，crc32=${hex32(state.imageCrc)}，modified=${new Date(file.lastModified).toLocaleString()}`);
      setControls();
    }

    function putU16(view, offset, value) {
      view.setUint16(offset, value & 0xFFFF, true);
    }

    function putU32(view, offset, value) {
      view.setUint32(offset, value >>> 0, true);
    }

    function buildStartFrame(imageSize, imageCrc, version, payloadSize) {
      const bytes = new Uint8Array(19);
      const view = new DataView(bytes.buffer);
      bytes[0] = FRAME_START;
      bytes[1] = 0x4F;
      bytes[2] = 0x54;
      bytes[3] = 0x41;
      bytes[4] = 0x31;
      putU32(view, 5, imageSize);
      putU32(view, 9, imageCrc);
      putU32(view, 13, version);
      putU16(view, 17, payloadSize);
      return bytes;
    }

    function buildDataFrame(seq, offset, payload) {
      const bytes = new Uint8Array(1 + 2 + 4 + 2 + payload.length + 4);
      const view = new DataView(bytes.buffer);
      bytes[0] = FRAME_DATA;
      putU16(view, 1, seq);
      putU32(view, 3, offset);
      putU16(view, 7, payload.length);
      bytes.set(payload, 9);
      putU32(view, 9 + payload.length, crc32(payload));
      return bytes;
    }

    function buildEndFrame(imageSize, imageCrc) {
      const bytes = new Uint8Array(9);
      const view = new DataView(bytes.buffer);
      bytes[0] = FRAME_END;
      putU32(view, 1, imageSize);
      putU32(view, 5, imageCrc);
      return bytes;
    }

    function parseAcks(value) {
      const acks = [];
      const bytes = new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
      state.ackByteBuffer.push(...bytes);

      while (state.ackByteBuffer.length > 0) {
        const start = state.ackByteBuffer.indexOf(FRAME_ACK);
        if (start < 0) {
          const nonPadding = state.ackByteBuffer.filter((byte) => byte !== 0x55);
          state.ackByteBuffer = nonPadding.slice(-8);
          break;
        }
        if (start > 0) {
          state.ackByteBuffer.splice(0, start);
        }
        if (state.ackByteBuffer.length < 9) {
          break;
        }
        {
          const ackLen = (state.ackByteBuffer.length >= 20 &&
            state.ackByteBuffer[9] === 0x44 &&
            state.ackByteBuffer[19] === 0xA5) ? 20 : 9;
          const frame = Uint8Array.from(state.ackByteBuffer.slice(0, ackLen));
          const view = new DataView(frame.buffer);
          const ack = {
            type: view.getUint8(1),
            status: view.getUint8(2),
            seq: view.getUint16(3, true),
            offset: view.getUint32(5, true)
          };
          if (ackLen === 20) {
            ack.diag = {
              rxState: view.getUint8(10),
              rxIndex: view.getUint16(11, true),
              rxExpected: view.getUint16(13, true),
              expectedSeq: view.getUint16(15, true),
              dropCount: view.getUint16(17, true)
            };
          }
          acks.push(ack);
          state.ackByteBuffer.splice(0, ackLen);
          while (state.ackByteBuffer[0] === 0x55) {
            state.ackByteBuffer.shift();
          }
        }
      }

      if (acks.length === 0) {
        state.ackTextBuffer = (state.ackTextBuffer + new TextDecoder().decode(bytes)).slice(-256);
      }
      if (state.ackByteBuffer.length > 512) {
        state.ackByteBuffer = state.ackByteBuffer.slice(-512);
      }
      const ackRegex = /\bACK\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s*(?:\r?\n|$)/ig;
      let match;
      let consumed = 0;
      while ((match = ackRegex.exec(state.ackTextBuffer)) !== null) {
        consumed = match.index + match[0].length;
        acks.push({
          type: Number(match[1]),
          status: Number(match[2]),
          seq: Number(match[3]),
          offset: Number(match[4])
        });
      }
      if (consumed > 0) {
        state.ackTextBuffer = state.ackTextBuffer.slice(consumed);
      }

      return acks;
    }

    function ackMatchesExpected(ack, expected) {
      return ack.type === expected.type &&
        (expected.seq === null || expected.seq === ack.seq || ack.status === 3);
    }

    function okAckMatchesExpected(ack, expected) {
      return ack.type === expected.type &&
        ack.status === 0 &&
        (expected.seq === null || expected.seq === ack.seq);
    }

    function formatBytes(bytes) {
      return Array.from(bytes).map((b) => b.toString(16).padStart(2, "0")).join(" ");
    }

    function formatAckDiag(diag) {
      if (!diag) {
        return "";
      }
      return ` diag{state=${diag.rxState} rx=${diag.rxIndex}/${diag.rxExpected} expSeq=${diag.expectedSeq} drops=${diag.dropCount}}`;
    }

    function formatAck(ack) {
      return `type=${ack.type} status=${ackStatusName(ack.status)} seq=${ack.seq} offset=${ack.offset}${formatAckDiag(ack.diag)}`;
    }

    function formatNotifyValue(value) {
      const bytes = new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
      const hex = formatBytes(bytes);
      let text = "";
      try {
        text = new TextDecoder().decode(bytes).replace(/\s+$/g, "");
      } catch (error) {
        text = "";
      }

      if (text && /^[\x09\x0A\x0D\x20-\x7E\u4E00-\u9FFF]+$/.test(text)) {
        return `${hex} | "${text}"`;
      }
      return hex;
    }

    function onNotification(event) {
      const notifyBytes = new Uint8Array(event.target.value.buffer, event.target.value.byteOffset, event.target.value.byteLength);
      state.notifyCount++;
      state.lastNotifyHex = formatBytes(notifyBytes);
      const acks = parseAcks(event.target.value);
      if (acks.length === 0) {
        state.lastNonAckNotifies.push(formatNotifyValue(event.target.value));
        state.lastNonAckNotifies = state.lastNonAckNotifies.slice(-6);
        if (state.inspectNextNotify) {
          state.inspectNextNotify = false;
          log(`检测到通知：${formatNotifyValue(event.target.value)}`);
          setControls();
        }
        return;
      }

      state.ackCount += acks.length;
      if (!state.pendingAck) {
        state.ackCache.push(...acks);
        state.ackCache = state.ackCache.slice(-32);
        return;
      }

      const expected = state.pendingAck;
      const okAck = acks.find((ack) => okAckMatchesExpected(ack, expected));
      if (okAck) {
        const elapsed = Math.round(performance.now() - expected.startedAt);
        okAck.elapsedMs = elapsed;
        if (elapsed > 1000) {
          log(`ACK 延迟 ${elapsed} ms：${formatAck(okAck)}`);
        }
        state.ackCache.push(...acks.filter((ack) => ack !== okAck));
        state.ackCache = state.ackCache.slice(-32);
        expected.resolve(okAck);
        state.pendingAck = null;
        return;
      }

      for (const ack of acks) {
        if (ackMatchesExpected(ack, expected)) {
          const elapsed = Math.round(performance.now() - expected.startedAt);
          ack.elapsedMs = elapsed;
          if (elapsed > 1000) {
            log(`ACK 延迟 ${elapsed} ms：${formatAck(ack)}`);
          }
          state.ackCache.push(...acks.filter((item) => item !== ack));
          state.ackCache = state.ackCache.slice(-32);
          expected.resolve(ack);
          state.pendingAck = null;
          return;
        }
      }

      state.ackCache.push(...acks);
      state.ackCache = state.ackCache.slice(-32);
    }

    function waitForAck(type, seq, timeoutMs) {
      if (state.pendingAck) {
        throw new Error("内部错误：仍有未完成 ACK");
      }
      throwIfStopRequested();

      const expected = { type, seq };
      let cachedIndex = state.ackCache.findIndex((ack) => okAckMatchesExpected(ack, expected));
      if (cachedIndex < 0) {
        cachedIndex = state.ackCache.findIndex((ack) => ackMatchesExpected(ack, expected));
      }
      if (cachedIndex >= 0) {
        const cachedAck = state.ackCache.splice(cachedIndex, 1)[0];
        log(`ACK cache hit：${formatAck(cachedAck)}`);
        return Promise.resolve(cachedAck);
      }

      return new Promise((resolve, reject) => {
        const timer = window.setTimeout(() => {
          if (state.ackTextBuffer) {
            log(`ACK timeout buffer="${state.ackTextBuffer.replace(/[\r\n]+/g, "\\n")}"`);
          }
          if (state.ackByteBuffer.length > 0) {
            log(`ACK byte buffer=${formatBytes(state.ackByteBuffer)}`);
          }
          log(`ACK stats notify=${state.notifyCount} ack=${state.ackCount} cache=${state.ackCache.length} lastNotify=${state.lastNotifyHex || "-"}`);
          if (state.lastNonAckNotifies.length > 0) {
            log(`last non-ACK notify: ${state.lastNonAckNotifies.join(" || ")}`);
          }
          if (state.pendingAck && state.pendingAck.resolve === resolve) {
            state.pendingAck = null;
          }
          reject(new Error("ACK timeout"));
        }, timeoutMs);

        state.pendingAck = {
          type,
          seq,
          startedAt: performance.now(),
          resolve: (ack) => {
            window.clearTimeout(timer);
            resolve(ack);
          },
          reject: (error) => {
            window.clearTimeout(timer);
            reject(error);
          }
        };
      });
    }

    async function waitForReplacementOkAck(type, seq, timeoutMs) {
      const deadline = performance.now() + timeoutMs;
      while (performance.now() < deadline) {
        const cachedIndex = state.ackCache.findIndex((ack) =>
          ack.type === type && ack.status === 0 && (seq === null || ack.seq === seq)
        );
        if (cachedIndex >= 0) {
          const ack = state.ackCache.splice(cachedIndex, 1)[0];
          log(`收到后续 OK ACK：${formatAck(ack)}`);
          return ack;
        }
        await sleep(20);
      }
      return null;
    }

    async function writeRawBytes(bytes) {
      throwIfStopRequested();
      const mode = els.writeMode.value;

      if (mode === "response") {
        if (!state.writeChar.properties.write) {
          throw new Error("写入特征不支持 writeValueWithResponse，请切换为自动或 without response");
        }
        if (state.writeChar.writeValueWithResponse) {
          await state.writeChar.writeValueWithResponse(bytes);
        } else {
          await state.writeChar.writeValue(bytes);
        }
        return;
      }

      if (mode === "without") {
        if (!state.writeChar.properties.writeWithoutResponse) {
          throw new Error("写入特征不支持 writeValueWithoutResponse，请切换为自动或 with response");
        }
        if (!state.writeChar.writeValueWithoutResponse) {
          throw new Error("当前浏览器不支持 writeValueWithoutResponse 方法，请使用新版 Chrome/Edge 或切换为 with response");
        }
        await state.writeChar.writeValueWithoutResponse(bytes);
        return;
      }

      if (state.writeChar.properties.writeWithoutResponse && state.writeChar.writeValueWithoutResponse) {
        await state.writeChar.writeValueWithoutResponse(bytes);
        return;
      }

      if (state.writeChar.properties.write && state.writeChar.writeValueWithResponse) {
        await state.writeChar.writeValueWithResponse(bytes);
        return;
      }

      if (state.writeChar.properties.write && state.writeChar.writeValue) {
        await state.writeChar.writeValue(bytes);
        return;
      }

      throw new Error(`写入特征不支持可用写入方式：${describeProperties(state.writeChar)}`);
    }

    async function writeBytes(bytes) {
      const chunkSize = state.tx.chunkSize;
      const chunkDelayMs = state.tx.chunkDelayMs;
      throwIfStopRequested();
      if (chunkSize <= 0 || bytes.length <= chunkSize) {
        await writeRawBytes(bytes);
        return;
      }

      for (let offset = 0; offset < bytes.length; offset += chunkSize) {
        throwIfStopRequested();
        await writeRawBytes(bytes.subarray(offset, Math.min(offset + chunkSize, bytes.length)));
        if (chunkDelayMs > 0 && offset + chunkSize < bytes.length) {
          await sleep(chunkDelayMs);
        }
      }
    }

    function calculateTailPadSize(frameLength) {
      const basePadSize = state.tx.tailPadSize;
      const chunkSize = state.tx.chunkSize;
      if (basePadSize <= 0) {
        return 0;
      }
      if (chunkSize <= 0) {
        return basePadSize;
      }

      const remainder = frameLength % chunkSize;
      const alignPadSize = remainder === 0 ? 0 : chunkSize - remainder;
      const guardPadSize = chunkSize * COMPAT_TAIL_PAD_GUARD_WRITES;
      return Math.max(basePadSize, alignPadSize + guardPadSize);
    }

    function appendTailPadding(frame) {
      const padSize = calculateTailPadSize(frame.length);
      if (padSize <= 0) {
        return frame;
      }

      const bytes = new Uint8Array(frame.length + padSize);
      bytes.set(frame, 0);
      bytes.fill(FRAME_TAIL_PAD, frame.length);
      return bytes;
    }

    function lowerTransferRate(reason) {
      const currentChunk = state.tx.chunkSize;
      const currentDelay = state.tx.chunkDelayMs;
      let nextChunk = currentChunk;
      let nextDelay = currentDelay;

      if (nextChunk > COMPAT_MIN_WRITE_SIZE) {
        nextChunk -= 1;
      } else {
        nextDelay = Math.min(Math.max(nextDelay + 20, 40), 120);
      }

      if (nextChunk !== currentChunk || nextDelay !== currentDelay) {
        state.tx.chunkSize = nextChunk;
        state.tx.chunkDelayMs = nextDelay;
        els.writeChunk.value = String(nextChunk);
        els.chunkDelay.value = String(nextDelay);
        log(`${reason}，自动降速：writeChunk=${nextChunk} chunkDelay=${nextDelay}ms`);
      }
    }

    async function sendWithAck(frame, type, seq, description) {
      const timeoutMs = readUintInput(els.ackTimeout, 5000);
      const retries = readUintInput(els.retry, 5);
      const ackDelayMs = state.tx.ackDelayMs;
      let lastError = null;

      for (let attempt = 0; attempt <= retries; attempt++) {
        throwIfStopRequested();
        try {
          state.ackCache = state.ackCache.filter((ack) =>
            !(ack.type === type && (seq === null || ack.seq === seq))
          );
          const txFrame = appendTailPadding(frame);
          const actualTailPadSize = txFrame.length - frame.length;
          if (attempt === 0 && (type !== FRAME_DATA || seq === 0 || (seq % 64) === 0)) {
            log(`${description} frame=${frame.length} tailPad=${actualTailPadSize} writeChunk=${state.tx.chunkSize} chunkDelay=${state.tx.chunkDelayMs}`);
          }
          const ackPromise = waitForAck(type, seq, timeoutMs);
          await writeBytes(txFrame);
          throwIfStopRequested();
          let ack = await ackPromise;
          if (ack.status !== 0) {
            if (ack.status === RX_TIMEOUT_STATUS) {
              lowerTransferRate(`${description} 收到 RX_TIMEOUT`);
              const replacementAck = await waitForReplacementOkAck(type, seq, Math.min(800, timeoutMs));
              if (replacementAck) {
                ack = replacementAck;
              } else {
                throw new Error(`设备返回 NAK ${formatAck(ack)}`);
              }
            } else {
              throw new Error(`设备返回 NAK ${formatAck(ack)}`);
            }
          }
          if (type === FRAME_START) {
            log(`START ACK reset_csr=${describeResetCsr(ack.offset)}`);
          }
          if (attempt > 0) {
            state.retryCount++;
            log(`${description} 重试成功 attempt=${attempt + 1} elapsed=${Math.round(ack.elapsedMs || 0)} ms ${formatAck(ack)}`);
          }
          if (ackDelayMs > 0) {
            await sleep(ackDelayMs);
          }
          return ack;
        } catch (error) {
          state.pendingAck = null;
          lastError = error;
          if (attempt >= retries) {
            break;
          }
          if (type === FRAME_DATA && /ACK timeout/i.test(error.message)) {
            lowerTransferRate(`${description} ${error.message}`);
          }
          log(`${description} 重试 ${attempt + 1}/${retries}：${error.message}`);
        }
      }

      throw new Error(`${description} 失败：${lastError ? lastError.message : "未知错误"}`);
    }

    async function connectBle() {
      if (!("bluetooth" in navigator)) {
        throw new Error("当前浏览器不支持 Web Bluetooth。请使用 Android/Windows/macOS 上的 Chrome 或 Edge。");
      }

      const serviceUuid = normalizeUuid(els.serviceUuid.value);
      const writeUuid = normalizeUuid(els.writeUuid.value);
      const notifyUuid = normalizeUuid(els.notifyUuid.value);
      const namePrefix = els.namePrefix.value.trim();
      const options = namePrefix
        ? { filters: [{ namePrefix }], optionalServices: [serviceUuid] }
        : { acceptAllDevices: true, optionalServices: [serviceUuid] };

      log("打开 BLE 设备选择器");
      state.device = await navigator.bluetooth.requestDevice(options);
      state.device.addEventListener("gattserverdisconnected", () => {
        state.server = null;
        state.writeChar = null;
        state.notifyChar = null;
        state.inspectNextNotify = false;
        state.pendingAck = null;
        setStatus("已断开", "error");
        setControls();
        log("BLE 已断开");
      });

      log(`连接 ${state.device.name || state.device.id}`);
      state.server = await state.device.gatt.connect();
      const service = await state.server.getPrimaryService(serviceUuid);
      state.writeChar = await service.getCharacteristic(writeUuid);
      state.notifyChar = await service.getCharacteristic(notifyUuid);
      await state.notifyChar.startNotifications();
      state.notifyChar.addEventListener("characteristicvaluechanged", onNotification);

      setStatus(`已连接：${state.device.name || "BLE device"}`, "connected");
      setControls();
      saveCurrentUuidHistories();
      log(`写入特征属性：${describeProperties(state.writeChar)}`);
      log(`通知特征属性：${describeProperties(state.notifyChar)}`);
      log("GATT 服务和特征已就绪");
    }

    async function disconnectBle() {
      if (state.device && state.device.gatt.connected) {
        state.device.gatt.disconnect();
      }
    }

    function inspectOneNotification() {
      if (!state.notifyChar) {
        log("尚未连接通知特征");
        return;
      }
      state.inspectNextNotify = true;
      setControls();
      log("已开启一次性通知检测：下一条非 ACK 通知会显示在日志中");
    }

    function stopOta() {
      if (!state.otaRunning) {
        return;
      }

      state.stopRequested = true;
      if (state.pendingAck && state.pendingAck.reject) {
        state.pendingAck.reject(new Error("用户已停止 OTA"));
        state.pendingAck = null;
      }
      setControls();
      log("已请求停止 OTA，当前传输会尽快中断");
    }

    async function startOta() {
      if (!state.fileBytes || state.imageCrc === null) {
        throw new Error("请先选择 app.bin");
      }

      let payloadSize = readUintInput(els.payloadSize, 64);
      if (payloadSize <= 0 || payloadSize > 820) {
        throw new Error("payload 字节数必须在 1 到 820 之间");
      }

      const writeChunkSize = readUintInput(els.writeChunk, COMPAT_SAFE_WRITE_SIZE);
      const chunkDelayMs = readUintInput(els.chunkDelay, 20);
      const ackDelayMs = readUintInput(els.ackDelay, 150);
      if (els.compatMode.checked && writeChunkSize > 0 && writeChunkSize < COMPAT_MIN_WRITE_SIZE) {
        throw new Error("单 BLE 写包兼容模式要求 Write chunk bytes 至少为 14");
      }

      const version = readUintInput(els.version, 1) >>> 0;
      const bytes = state.fileBytes;
      const imageCrc = state.imageCrc >>> 0;
      let seq = 0;

      state.otaRunning = true;
      state.stopRequested = false;
      state.startedAt = performance.now();
      state.tx.chunkSize = readUintInput(els.writeChunk, COMPAT_SAFE_WRITE_SIZE);
      state.tx.chunkDelayMs = chunkDelayMs;
      state.tx.ackDelayMs = ackDelayMs;
      state.tx.tailPadSize = els.compatMode.checked ? COMPAT_TAIL_PAD_SIZE : 0;
      updateProgress(0);
      setControls();

      try {
        throwIfStopRequested();
        log(`START image_size=${bytes.length} image_crc=${hex32(imageCrc)} version=${version} payload=${payloadSize}`);
        await sendWithAck(buildStartFrame(bytes.length, imageCrc, version, payloadSize), FRAME_START, null, "START");

        for (let offset = 0; offset < bytes.length;) {
          throwIfStopRequested();
          let currentPayloadSize = Math.min(payloadSize, bytes.length - offset);
          let payload = bytes.subarray(offset, offset + currentPayloadSize);
          const frame = buildDataFrame(seq, offset, payload);
          try {
            await sendWithAck(frame, FRAME_DATA, seq, `DATA seq=${seq} offset=${offset}`);
          } catch (error) {
            if (els.compatMode.checked &&
                currentPayloadSize > COMPAT_MIN_PAYLOAD_SIZE &&
                /RX_TIMEOUT|ACK timeout/i.test(error.message)) {
              currentPayloadSize = Math.max(COMPAT_MIN_PAYLOAD_SIZE, Math.floor(currentPayloadSize / 2));
              payload = bytes.subarray(offset, offset + currentPayloadSize);
              log(`DATA seq=${seq} offset=${offset} 改用短 payload=${currentPayloadSize} 重发`);
              await sendWithAck(buildDataFrame(seq, offset, payload), FRAME_DATA, seq, `DATA seq=${seq} offset=${offset} short`);
            } else {
              throw error;
            }
          }
          updateProgress(offset + payload.length);
          offset += payload.length;
          seq = (seq + 1) & 0xFFFF;
        }

        throwIfStopRequested();
        log("END，等待设备校验下载区 CRC 并写 metadata");
        await sendWithAck(buildEndFrame(bytes.length, imageCrc), FRAME_END, null, "END");
        updateProgress(bytes.length);
        log("OTA 完成。设备端现在可以写 UPDATE_PENDING metadata 并复位。");
      } finally {
        state.otaRunning = false;
        state.stopRequested = false;
        state.pendingAck = null;
        setControls();
      }
    }

    els.file.addEventListener("change", () => {
      loadSelectedFile().catch((error) => {
        log(`文件错误：${error.message}`);
        setStatus("文件错误", "error");
      });
    });

    els.connect.addEventListener("click", () => {
      connectBle().catch((error) => {
        log(`连接失败：${error.message}`);
        setStatus("连接失败", "error");
        setControls();
      });
    });

    els.disconnect.addEventListener("click", () => {
      disconnectBle().catch((error) => log(`断开失败：${error.message}`));
    });

    els.inspectNotify.addEventListener("click", () => {
      inspectOneNotification();
    });

    els.stop.addEventListener("click", () => {
      stopOta();
    });

    els.start.addEventListener("click", () => {
      startOta().catch((error) => {
        log(`OTA 失败：${error.message}`);
        setStatus("OTA 失败", "error");
      });
    });

    els.clearLog.addEventListener("click", () => {
      els.log.textContent = "";
    });

    els.copyLog.addEventListener("click", () => {
      copyLogText().catch((error) => log(`复制日志失败：${error.message}`));
    });

    els.serviceUuid.addEventListener("change", saveCurrentUuidHistories);
    els.writeUuid.addEventListener("change", saveCurrentUuidHistories);
    els.notifyUuid.addEventListener("change", saveCurrentUuidHistories);

    loadUuidHistories();
    setControls();
    log("页面已加载。Web Bluetooth 需要 HTTPS 或 localhost，并且连接按钮必须由点击触发。");
