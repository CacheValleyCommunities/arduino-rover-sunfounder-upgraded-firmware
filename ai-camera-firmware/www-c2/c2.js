(function () {
  const SETTING_FIELDS = [
    ["follow_stop_cm", "Follow stop (cm)", 1, 100],
    ["follow_slow_cm", "Follow slow (cm)", 1, 200],
    ["follow_distance_cm", "Follow distance (cm)", 1, 300],
    ["follow_slow_power", "Follow slow power", 0, 100],
    ["avoid_threshold_cm", "Avoid threshold (cm)", 1, 300],
    ["reverse_cm", "Reverse (cm)", 1, 100],
    ["move_power", "Move power", 0, 100],
    ["ms_per_foot", "Ms per foot", 100, 10000],
    ["ms_per_deg", "Ms per degree", 1, 100],
  ];

  const COLOR_RGB = {
    red: [255, 2, 2],
    orange: [255, 165, 0],
    yellow: [255, 255, 10],
    green: [10, 255, 10],
    cyan: [10, 255, 255],
    blue: [10, 10, 255],
    purple: [165, 10, 255],
    magenta: [255, 10, 165],
    white: [255, 255, 255],
    off: [0, 0, 0],
  };

  const BLOCK_HELP = {
    rover_on_start: {
      title: "when robot turns on",
      body: "Hat for the script that should run. Run and Deploy only start stacks hanging off this block if one exists.",
    },
    rover_wait: {
      title: "wait",
      body: "Pause this many seconds. Stop cancels the wait.",
    },
    rover_repeat: {
      title: "repeat",
      body: "Run the inner stack a fixed number of times.",
    },
    rover_forever: {
      title: "forever",
      body: "Loop until Stop, stop all, or the page closes (live Run only). Deployed forever keeps going after you leave.",
    },
    rover_if: {
      title: "if",
      body: "Run the inner stack when the condition is true. Plug a compare / IR / number into the hex input.",
    },
    rover_ifelse: {
      title: "if / else",
      body: "One stack when the condition is true, the other when it is false.",
    },
    rover_wait_until: {
      title: "wait until",
      body: "Keep checking sensors until the condition is true, then continue.",
    },
    rover_stop_all: {
      title: "stop all",
      body: "Zero the motors and end the script, live or onboard.",
    },
    rover_distance: {
      title: "distance (cm)",
      body: "Ultrasonic ping in centimetres. −1 means no echo (too close, too far, or pointed at the sky). Pair with compare, e.g. distance < 20.",
    },
    rover_ir_left: {
      title: "IR left",
      body: "True when the left bumper IR sees an obstacle (the pin reads LOW). False means the path on that side is clear.",
    },
    rover_ir_right: {
      title: "IR right",
      body: "True when the right bumper IR sees an obstacle. Use with if or wait until to turn away from a wall.",
    },
    rover_battery: {
      title: "battery (V)",
      body: "Pack voltage, about 6.6–8.4 V. Not a percent. HUD shows the same value.",
    },
    rover_threshold: {
      title: "threshold",
      body: "Named follow/avoid distances from the Thresholds sidebar. Change the sliders, Apply settings, then compare against distance.",
    },
    rover_compare: {
      title: "compare",
      body: "True/false test. Sensors and numbers plug into both sides. IR blocks are 1 when blocked and 0 when clear.",
    },
    rover_number: {
      title: "number",
      body: "A constant. Snap it into wait, move, compare, look, or RGB.",
    },
    rover_move: {
      title: "move",
      body: "Drive straight for time, feet, or cm using move power and ms per foot from Thresholds.",
    },
    rover_turn: {
      title: "turn",
      body: "Spin in place. Degrees × ms per degree in Thresholds sets how long it turns.",
    },
    rover_motors_stop: {
      title: "stop motors",
      body: "Zero both motors but keep the rest of the script running.",
    },
    rover_set_power: {
      title: "set move power",
      body: "0–100. Changes the speed of later move/turn blocks.",
    },
    rover_lights: {
      title: "set lights",
      body: "RGB strip color. Off is black. Custom RGB is the block with R G B numbers.",
    },
    rover_rgb: {
      title: "set lights RGB",
      body: "Raw 0–255 per channel on the six-LED strip (not the tiny status LED on the CAM).",
    },
    rover_lamp: {
      title: "lamp",
      body: "ESP32-CAM flash on the camera. Separate from the body RGB strip.",
    },
    rover_look: {
      title: "look",
      body: "Camera servo 0–140°. 90° is center.",
    },
  };

  function abortError() {
    const err = new Error("Stopped");
    err.name = "AbortError";
    return err;
  }

  const rover = {
    abort: null,
    state: {
      sensors: { distance_cm: null, ir_left: false, ir_right: false, battery_v: 0 },
      settings: {},
      running: false,
    },
    throwIfAborted() {
      if (this.abort && this.abort.aborted) {
        throw abortError();
      }
    },
    async request(path, options) {
      const opts = options ? Object.assign({}, options) : {};
      const ignoreAbort = !!opts.ignoreAbort;
      delete opts.ignoreAbort;
      if (!ignoreAbort) {
        this.throwIfAborted();
        if (this.abort) {
          opts.signal = this.abort.signal;
        }
      }
      try {
        const res = await fetch(path, opts);
        if (!ignoreAbort) {
          this.throwIfAborted();
        }
        return res;
      } catch (err) {
        if (err && err.name === "AbortError") {
          throw abortError();
        }
        throw err;
      }
    },
    async refresh() {
      const res = await this.request("/c2/api/state", { ignoreAbort: !this.abort });
      this.state = await res.json();
      return this.state;
    },
    async post(path, body) {
      const res = await this.request(path, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(body || {}),
      });
      if (!res.ok) {
        throw new Error(path + " " + res.status);
      }
    },
    async heartbeat() {
      try {
        await this.request("/c2/api/heartbeat", { method: "POST" });
      } catch (err) {
        if (!err || err.name !== "AbortError") {
          throw err;
        }
      }
    },
    async motors(left, right) {
      await this.post("/c2/api/motors", { left, right });
    },
    async stop() {
      await fetch("/c2/api/stop", { method: "POST" });
    },
    async sleep(ms) {
      const signal = this.abort && this.abort.signal;
      const deadline = Date.now() + Number(ms);
      while (Date.now() < deadline) {
        this.throwIfAborted();
        const slice = Math.min(50, deadline - Date.now());
        await new Promise((resolve, reject) => {
          const timer = setTimeout(() => {
            if (signal) {
              signal.removeEventListener("abort", onAbort);
            }
            resolve();
          }, slice);
          function onAbort() {
            clearTimeout(timer);
            reject(abortError());
          }
          if (signal) {
            if (signal.aborted) {
              onAbort();
              return;
            }
            signal.addEventListener("abort", onAbort);
          }
        });
      }
    },
    async move(dir, amount, unit) {
      await this.refresh();
      const power = Number(this.state.settings.move_power || 70);
      const sign = dir === "backward" ? -1 : 1;
      let ms = Number(amount) * 1000;
      const msFoot = Number(this.state.settings.ms_per_foot || 1800);
      if (unit === "ft") {
        ms = Number(amount) * msFoot;
      } else if (unit === "cm") {
        ms = Number(amount) * (msFoot / 30.48);
      }
      await this.motors(sign * power, sign * power);
      try {
        await this.sleep(ms);
      } finally {
        await fetch("/c2/api/motors", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ left: 0, right: 0 }),
        });
      }
    },
    async turn(dir, deg) {
      await this.refresh();
      const power = Number(this.state.settings.move_power || 70);
      const ms = Math.abs(Number(deg)) * Number(this.state.settings.ms_per_deg || 8);
      const left = dir === "left" ? -power : power;
      const right = dir === "left" ? power : -power;
      await this.motors(left, right);
      try {
        await this.sleep(ms);
      } finally {
        await fetch("/c2/api/motors", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ left: 0, right: 0 }),
        });
      }
    },
  };

  function defineBlocks() {
    const blocks = [
      {
        type: "rover_on_start",
        message0: "when robot turns on",
        nextStatement: null,
        colour: 35,
        hat: "cap",
      },
      {
        type: "rover_wait",
        message0: "wait %1 seconds",
        args0: [{ type: "input_value", name: "SECS", check: "Number" }],
        previousStatement: null,
        nextStatement: null,
        colour: 210,
      },
      {
        type: "rover_repeat",
        message0: "repeat %1 times",
        args0: [{ type: "input_value", name: "TIMES", check: "Number" }],
        message1: "do %1",
        args1: [{ type: "input_statement", name: "DO" }],
        previousStatement: null,
        nextStatement: null,
        colour: 210,
      },
      {
        type: "rover_forever",
        message0: "forever",
        message1: "do %1",
        args1: [{ type: "input_statement", name: "DO" }],
        previousStatement: null,
        colour: 210,
      },
      {
        type: "rover_if",
        message0: "if %1",
        args0: [{ type: "input_value", name: "COND", check: "Boolean" }],
        message1: "do %1",
        args1: [{ type: "input_statement", name: "DO" }],
        previousStatement: null,
        nextStatement: null,
        colour: 210,
      },
      {
        type: "rover_ifelse",
        message0: "if %1",
        args0: [{ type: "input_value", name: "COND", check: "Boolean" }],
        message1: "do %1",
        args1: [{ type: "input_statement", name: "DO" }],
        message2: "else %1",
        args2: [{ type: "input_statement", name: "ELSE" }],
        previousStatement: null,
        nextStatement: null,
        colour: 210,
      },
      {
        type: "rover_wait_until",
        message0: "wait until %1",
        args0: [{ type: "input_value", name: "COND", check: "Boolean" }],
        previousStatement: null,
        nextStatement: null,
        colour: 210,
      },
      {
        type: "rover_stop_all",
        message0: "stop all",
        previousStatement: null,
        colour: 210,
      },
      {
        type: "rover_distance",
        message0: "distance (cm)",
        output: "Number",
        colour: 180,
      },
      {
        type: "rover_ir_left",
        message0: "IR left",
        output: "Boolean",
        colour: 180,
      },
      {
        type: "rover_ir_right",
        message0: "IR right",
        output: "Boolean",
        colour: 180,
      },
      {
        type: "rover_battery",
        message0: "battery (V)",
        output: "Number",
        colour: 180,
      },
      {
        type: "rover_threshold",
        message0: "threshold %1",
        args0: [{
          type: "field_dropdown",
          name: "KEY",
          options: [
            ["follow stop", "follow_stop_cm"],
            ["follow slow", "follow_slow_cm"],
            ["follow distance", "follow_distance_cm"],
            ["avoid", "avoid_threshold_cm"],
            ["reverse", "reverse_cm"],
          ],
        }],
        output: "Number",
        colour: 180,
      },
      {
        type: "rover_compare",
        message0: "%1 %2 %3",
        args0: [
          { type: "input_value", name: "A" },
          {
            type: "field_dropdown",
            name: "OP",
            options: [["=", "EQ"], ["≠", "NEQ"], ["<", "LT"], ["≤", "LTE"], [">", "GT"], ["≥", "GTE"]],
          },
          { type: "input_value", name: "B" },
        ],
        output: "Boolean",
        colour: 180,
        inputsInline: true,
      },
      {
        type: "rover_number",
        message0: "%1",
        args0: [{ type: "field_number", name: "NUM", value: 1 }],
        output: "Number",
        colour: 230,
      },
      {
        type: "rover_move",
        message0: "move %1 %2 %3",
        args0: [
          {
            type: "field_dropdown",
            name: "DIR",
            options: [["forward", "forward"], ["backward", "backward"]],
          },
          { type: "input_value", name: "AMOUNT", check: "Number" },
          {
            type: "field_dropdown",
            name: "UNIT",
            options: [["seconds", "sec"], ["feet", "ft"], ["cm", "cm"]],
          },
        ],
        previousStatement: null,
        nextStatement: null,
        colour: 120,
        inputsInline: true,
      },
      {
        type: "rover_turn",
        message0: "turn %1 %2 deg",
        args0: [
          {
            type: "field_dropdown",
            name: "DIR",
            options: [["left", "left"], ["right", "right"]],
          },
          { type: "input_value", name: "DEG", check: "Number" },
        ],
        previousStatement: null,
        nextStatement: null,
        colour: 120,
        inputsInline: true,
      },
      {
        type: "rover_motors_stop",
        message0: "stop motors",
        previousStatement: null,
        nextStatement: null,
        colour: 120,
      },
      {
        type: "rover_set_power",
        message0: "set move power %1",
        args0: [{ type: "input_value", name: "POWER", check: "Number" }],
        previousStatement: null,
        nextStatement: null,
        colour: 120,
      },
      {
        type: "rover_lights",
        message0: "set lights %1",
        args0: [{
          type: "field_dropdown",
          name: "COLOR",
          options: [
            ["red", "red"],
            ["orange", "orange"],
            ["yellow", "yellow"],
            ["green", "green"],
            ["cyan", "cyan"],
            ["blue", "blue"],
            ["purple", "purple"],
            ["magenta", "magenta"],
            ["white", "white"],
            ["off", "off"],
          ],
        }],
        previousStatement: null,
        nextStatement: null,
        colour: 120,
      },
      {
        type: "rover_rgb",
        message0: "set lights RGB R %1 G %2 B %3",
        args0: [
          { type: "input_value", name: "R", check: "Number" },
          { type: "input_value", name: "G", check: "Number" },
          { type: "input_value", name: "B", check: "Number" },
        ],
        previousStatement: null,
        nextStatement: null,
        colour: 120,
        inputsInline: true,
      },
      {
        type: "rover_lamp",
        message0: "lamp %1",
        args0: [{
          type: "field_dropdown",
          name: "ON",
          options: [["on", "1"], ["off", "0"]],
        }],
        previousStatement: null,
        nextStatement: null,
        colour: 120,
      },
      {
        type: "rover_look",
        message0: "look %1 deg",
        args0: [{ type: "input_value", name: "ANGLE", check: "Number" }],
        previousStatement: null,
        nextStatement: null,
        colour: 120,
      },
    ];
    blocks.forEach((block) => {
      const help = BLOCK_HELP[block.type];
      if (help) {
        block.tooltip = help.title + " — " + help.body;
      }
    });
    Blockly.defineBlocksWithJsonArray(blocks);
  }

  function toolbox() {
    const shadowNumber = (value) => ({
      shadow: { type: "rover_number", fields: { NUM: value } },
    });
    return {
      kind: "categoryToolbox",
      contents: [
        {
          kind: "category",
          name: "Control",
          colour: "210",
          contents: [
            { kind: "block", type: "rover_on_start" },
            { kind: "block", type: "rover_wait", inputs: { SECS: shadowNumber(1) } },
            { kind: "block", type: "rover_repeat", inputs: { TIMES: shadowNumber(4) } },
            { kind: "block", type: "rover_forever" },
            { kind: "block", type: "rover_if" },
            { kind: "block", type: "rover_ifelse" },
            { kind: "block", type: "rover_wait_until" },
            { kind: "block", type: "rover_stop_all" },
          ],
        },
        {
          kind: "category",
          name: "Sensor",
          colour: "180",
          contents: [
            { kind: "block", type: "rover_distance" },
            { kind: "block", type: "rover_ir_left" },
            { kind: "block", type: "rover_ir_right" },
            { kind: "block", type: "rover_battery" },
            { kind: "block", type: "rover_threshold" },
            { kind: "block", type: "rover_compare" },
            { kind: "block", type: "rover_number" },
          ],
        },
        {
          kind: "category",
          name: "Move",
          colour: "120",
          contents: [
            { kind: "block", type: "rover_move", inputs: { AMOUNT: shadowNumber(1) } },
            { kind: "block", type: "rover_turn", inputs: { DEG: shadowNumber(90) } },
            { kind: "block", type: "rover_motors_stop" },
            { kind: "block", type: "rover_set_power", inputs: { POWER: shadowNumber(70) } },
            { kind: "block", type: "rover_lights" },
            {
              kind: "block",
              type: "rover_rgb",
              inputs: {
                R: shadowNumber(255),
                G: shadowNumber(0),
                B: shadowNumber(0),
              },
            },
            { kind: "block", type: "rover_lamp" },
            { kind: "block", type: "rover_look", inputs: { ANGLE: shadowNumber(90) } },
          ],
        },
      ],
    };
  }

  function genValue(block, name, fallback) {
    const target = block.getInputTargetBlock(name);
    if (!target) {
      return fallback;
    }
    return genExpr(target);
  }

  function genExpr(block) {
    switch (block.type) {
      case "rover_number":
        return String(Number(block.getFieldValue("NUM")));
      case "rover_distance":
        return "(rover.state.sensors.distance_cm)";
      case "rover_ir_left":
        return "(rover.state.sensors.ir_left)";
      case "rover_ir_right":
        return "(rover.state.sensors.ir_right)";
      case "rover_battery":
        return "(rover.state.sensors.battery_v)";
      case "rover_threshold":
        return "(rover.state.settings." + block.getFieldValue("KEY") + ")";
      case "rover_compare": {
        const ops = { EQ: "===", NEQ: "!==", LT: "<", LTE: "<=", GT: ">", GTE: ">=" };
        return "(" + genValue(block, "A", "0") + " " + ops[block.getFieldValue("OP")] + " " +
          genValue(block, "B", "0") + ")";
      }
      default:
        return "0";
    }
  }

  function genStack(block) {
    let code = "";
    for (let current = block; current; current = current.getNextBlock()) {
      code += genStatement(current);
    }
    return code;
  }

  function genStatement(block) {
    switch (block.type) {
      case "rover_on_start":
        return genStack(block.getNextBlock());
      case "rover_wait":
        return "await rover.sleep((" + genValue(block, "SECS", "1") + ") * 1000);\n";
      case "rover_repeat":
        return "for (let i = 0; i < (" + genValue(block, "TIMES", "1") + "); i++) {\n" +
          "rover.throwIfAborted();\nawait rover.refresh();\n" +
          genStack(block.getInputTargetBlock("DO")) + "}\n";
      case "rover_forever":
        return "while (true) {\nrover.throwIfAborted();\nawait rover.refresh();\n" +
          genStack(block.getInputTargetBlock("DO")) + "}\n";
      case "rover_if":
        return "await rover.refresh();\nif (" + genValue(block, "COND", "false") + ") {\n" +
          genStack(block.getInputTargetBlock("DO")) + "}\n";
      case "rover_ifelse":
        return "await rover.refresh();\nif (" + genValue(block, "COND", "false") + ") {\n" +
          genStack(block.getInputTargetBlock("DO")) + "} else {\n" +
          genStack(block.getInputTargetBlock("ELSE")) + "}\n";
      case "rover_wait_until":
        return "while (true) {\nrover.throwIfAborted();\nawait rover.refresh();\n" +
          "if (" + genValue(block, "COND", "false") + ") { break; }\nawait rover.sleep(80);\n}\n";
      case "rover_stop_all":
        return "await rover.stop();\nthrow new DOMException('Stopped', 'AbortError');\n";
      case "rover_move":
        return "await rover.move(" + JSON.stringify(block.getFieldValue("DIR")) + ", " +
          genValue(block, "AMOUNT", "1") + ", " + JSON.stringify(block.getFieldValue("UNIT")) + ");\n";
      case "rover_turn":
        return "await rover.turn(" + JSON.stringify(block.getFieldValue("DIR")) + ", " +
          genValue(block, "DEG", "90") + ");\n";
      case "rover_motors_stop":
        return "await rover.motors(0, 0);\n";
      case "rover_set_power":
        return "await rover.post('/c2/api/settings', { move_power: " +
          genValue(block, "POWER", "70") + " });\nawait rover.refresh();\n";
      case "rover_lights": {
        const rgb = COLOR_RGB[block.getFieldValue("COLOR")] || COLOR_RGB.off;
        return "await rover.post('/c2/api/rgb', { r: " + rgb[0] +
          ", g: " + rgb[1] + ", b: " + rgb[2] + " });\n";
      }
      case "rover_rgb":
        return "await rover.post('/c2/api/rgb', { r: " + genValue(block, "R", "0") +
          ", g: " + genValue(block, "G", "0") + ", b: " + genValue(block, "B", "0") + " });\n";
      case "rover_lamp":
        return "await rover.post('/c2/api/lamp', { on: " +
          (block.getFieldValue("ON") === "1") + " });\n";
      case "rover_look":
        return "await rover.post('/c2/api/servo', { angle: " +
          genValue(block, "ANGLE", "90") + " });\n";
      default:
        return "";
    }
  }

  function jsonValue(block) {
    if (!block) {
      return null;
    }
    switch (block.type) {
      case "rover_number":
        return Number(block.getFieldValue("NUM"));
      case "rover_distance":
        return "distance_cm";
      case "rover_ir_left":
        return "ir_left";
      case "rover_ir_right":
        return "ir_right";
      case "rover_battery":
        return "battery_v";
      case "rover_threshold":
        return block.getFieldValue("KEY");
      case "rover_compare":
        return {
          op: block.getFieldValue("OP").toLowerCase(),
          a: jsonValue(block.getInputTargetBlock("A")),
          b: jsonValue(block.getInputTargetBlock("B")),
        };
      default:
        return block.type;
    }
  }

  function jsonStack(block) {
    const steps = [];
    for (let current = block; current; current = current.getNextBlock()) {
      steps.push(jsonStatement(current));
    }
    return steps;
  }

  function jsonStatement(block) {
    switch (block.type) {
      case "rover_on_start":
        return { when: "start", do: jsonStack(block.getNextBlock()) };
      case "rover_wait":
        return { wait: jsonValue(block.getInputTargetBlock("SECS")) + "s" };
      case "rover_repeat":
        return { repeat: jsonValue(block.getInputTargetBlock("TIMES")), steps: jsonStack(block.getInputTargetBlock("DO")) };
      case "rover_forever":
        return { forever: jsonStack(block.getInputTargetBlock("DO")) };
      case "rover_if":
        return { if: jsonValue(block.getInputTargetBlock("COND")), do: jsonStack(block.getInputTargetBlock("DO")) };
      case "rover_ifelse":
        return {
          if: jsonValue(block.getInputTargetBlock("COND")),
          do: jsonStack(block.getInputTargetBlock("DO")),
          else: jsonStack(block.getInputTargetBlock("ELSE")),
        };
      case "rover_wait_until":
        return { wait_until: jsonValue(block.getInputTargetBlock("COND")) };
      case "rover_stop_all":
        return { stop: "all" };
      case "rover_move":
        return { ["move_" + block.getFieldValue("DIR")]: jsonValue(block.getInputTargetBlock("AMOUNT")) + block.getFieldValue("UNIT") };
      case "rover_turn":
        return { turn: block.getFieldValue("DIR") + " " + jsonValue(block.getInputTargetBlock("DEG")) + "deg" };
      case "rover_motors_stop":
        return { stop: "motors" };
      case "rover_set_power":
        return { set_power: jsonValue(block.getInputTargetBlock("POWER")) };
      case "rover_lights": {
        const rgb = COLOR_RGB[block.getFieldValue("COLOR")] || COLOR_RGB.off;
        return { set_color: [rgb[0], rgb[1], rgb[2]] };
      }
      case "rover_rgb":
        return { set_color: [jsonValue(block.getInputTargetBlock("R")), jsonValue(block.getInputTargetBlock("G")), jsonValue(block.getInputTargetBlock("B"))] };
      case "rover_lamp":
        return { lamp: block.getFieldValue("ON") === "1" ? "on" : "off" };
      case "rover_look":
        return { look: jsonValue(block.getInputTargetBlock("ANGLE")) };
      default:
        return { op: block.type };
    }
  }

  let workspace;

  function entryBlocks() {
    const tops = workspace.getTopBlocks(true).filter((block) => !block.outputConnection);
    const hats = tops.filter((block) => block.type === "rover_on_start");
    return hats.length ? hats : tops;
  }

  function generateProgram() {
    const entries = entryBlocks();
    let code = "";
    const json = [];
    entries.forEach((block) => {
      if (block.type === "rover_on_start") {
        code += genStack(block.getNextBlock());
        json.push(jsonStatement(block));
      } else {
        code += genStack(block);
        json.push(...jsonStack(block));
      }
    });
    return { code, json: { version: 1, steps: json } };
  }

  function renderSettings() {
    const root = document.getElementById("settings");
    root.innerHTML = "";
    SETTING_FIELDS.forEach(([key, label, min, max]) => {
      const wrap = document.createElement("label");
      wrap.className = "setting";
      const value = rover.state.settings[key];
      wrap.innerHTML = "<span>" + label + "</span><strong>" + (value ?? "—") + "</strong>" +
        "<input type='range' min='" + min + "' max='" + max + "' name='" + key + "' value='" + (value ?? min) + "' />";
      const input = wrap.querySelector("input");
      const strong = wrap.querySelector("strong");
      input.addEventListener("input", () => {
        strong.textContent = input.value;
      });
      root.appendChild(wrap);
    });
  }

  function renderHud() {
    const dist = rover.state.sensors.distance_cm;
    document.querySelector("[data-k=distance]").textContent =
      "DIST " + (dist == null || dist < 0 ? "—" : Number(dist).toFixed(0) + "cm");
    document.querySelector("[data-k=ir]").textContent =
      "IR " + (rover.state.sensors.ir_left ? "L" : "·") + " " +
      (rover.state.sensors.ir_right ? "R" : "·");
    const volts = rover.state.sensors.battery_v;
    document.querySelector("[data-k=battery]").textContent =
      "BAT " + (volts ? Number(volts).toFixed(2) + "V" : "—");
    let mode = "IDLE";
    if (rover.state.onboard) {
      mode = "ONBOARD";
    } else if (rover.abort) {
      mode = "LIVE";
    } else if (rover.state.deployed) {
      mode = "SAVED";
    } else if (rover.state.running) {
      mode = "LIVE";
    }
    document.querySelector("[data-k=mode]").textContent = mode;
    const live = !!rover.abort;
    const onboard = !!rover.state.onboard;
    const running = !!rover.state.running;
    document.getElementById("btnStop").disabled = !(live || onboard || running);
    document.getElementById("btnRun").disabled = live;
    document.getElementById("btnDeploy").disabled = live;
    renderPreview();
  }

  function worldToCanvas(preview, x, y, width, height, pad) {
    const b = preview.bounds;
    const spanX = Math.max(1, b.x1 - b.x0);
    const spanY = Math.max(1, b.y1 - b.y0);
    return {
      x: pad + ((x - b.x0) / spanX) * (width - pad * 2),
      y: height - pad - ((y - b.y0) / spanY) * (height - pad * 2),
    };
  }

  function renderPreview() {
    const panel = document.getElementById("simPanel");
    const canvas = document.getElementById("simCanvas");
    const caption = document.getElementById("simCaption");
    if (!panel || !canvas) {
      return;
    }
    const preview = rover.state.preview;
    if (!preview || !preview.bounds) {
      panel.hidden = true;
      return;
    }
    panel.hidden = false;
    const ctx = canvas.getContext("2d");
    const dpr = window.devicePixelRatio || 1;
    const width = canvas.clientWidth || 256;
    const height = canvas.clientHeight || 176;
    if (canvas.width !== Math.round(width * dpr) || canvas.height !== Math.round(height * dpr)) {
      canvas.width = Math.round(width * dpr);
      canvas.height = Math.round(height * dpr);
    }
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, width, height);
    ctx.fillStyle = "#07090c";
    ctx.fillRect(0, 0, width, height);
    const pad = 10;
    const to = (x, y) => worldToCanvas(preview, x, y, width, height, pad);
    const b = preview.bounds;
    const nw = to(b.x0, b.y1);
    const se = to(b.x1, b.y0);
    ctx.strokeStyle = "rgba(255,255,255,0.16)";
    ctx.lineWidth = 1;
    ctx.strokeRect(nw.x, nw.y, se.x - nw.x, se.y - nw.y);
    const trail = preview.trail || [];
    if (trail.length > 1) {
      ctx.beginPath();
      trail.forEach((point, i) => {
        const p = to(point[0], point[1]);
        if (i === 0) {
          ctx.moveTo(p.x, p.y);
        } else {
          ctx.lineTo(p.x, p.y);
        }
      });
      ctx.strokeStyle = "rgba(108, 182, 255, 0.85)";
      ctx.lineWidth = 2;
      ctx.stroke();
    }
    const origin = to(preview.x_cm, preview.y_cm);
    const heading = (preview.heading_deg || 0) * Math.PI / 180;
    const dist = rover.state.sensors.distance_cm;
    if (dist != null && dist >= 0) {
      const beam = to(
        preview.x_cm + dist * Math.cos(heading),
        preview.y_cm + dist * Math.sin(heading)
      );
      ctx.beginPath();
      ctx.moveTo(origin.x, origin.y);
      ctx.lineTo(beam.x, beam.y);
      ctx.strokeStyle = "rgba(61, 214, 140, 0.55)";
      ctx.lineWidth = 1;
      ctx.stroke();
    }
    ctx.save();
    ctx.translate(origin.x, origin.y);
    // Canvas y is flipped vs world +y, so rotate the opposite way.
    ctx.rotate(-heading);
    const rgb = preview.rgb || [0, 0, 0];
    const body = rgb[0] || rgb[1] || rgb[2]
      ? "rgb(" + rgb[0] + "," + rgb[1] + "," + rgb[2] + ")"
      : "#8a9bb0";
    if (preview.lamp) {
      ctx.fillStyle = "rgba(255, 230, 140, 0.35)";
      ctx.beginPath();
      ctx.ellipse(16, 0, 18, 10, 0, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.fillStyle = body;
    ctx.strokeStyle = "#f4f6f8";
    ctx.lineWidth = 1.25;
    ctx.beginPath();
    ctx.moveTo(14, 0);
    ctx.lineTo(-10, 9);
    ctx.lineTo(-7, 0);
    ctx.lineTo(-10, -9);
    ctx.closePath();
    ctx.fill();
    ctx.stroke();
    const look = ((preview.servo == null ? 90 : preview.servo) - 90) * Math.PI / 180;
    ctx.save();
    ctx.rotate(-look);
    ctx.fillStyle = "#041018";
    ctx.fillRect(4, -3, 8, 6);
    ctx.restore();
    ctx.restore();
    if (caption) {
      const lookDeg = preview.servo == null ? 90 : preview.servo;
      caption.textContent = (preview.action || "stopped") +
        " · look " + lookDeg + "° · L" + (preview.left || 0) +
        " R" + (preview.right || 0);
    }
  }

  async function poll() {
    try {
      await rover.refresh();
      renderHud();
      if (!document.querySelector("#settings input:active")) {
        renderSettings();
      }
    } catch (err) {
      /* rover may be waking up */
    }
  }

  async function deployProgram() {
    stopProgram();
    const generated = generateProgram();
    document.getElementById("preview").textContent = JSON.stringify(generated.json, null, 2);
    const res = await fetch("/c2/api/program", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(generated.json),
    });
    if (!res.ok) {
      console.error("deploy failed", res.status);
    }
    await poll();
  }

  async function undeployProgram() {
    stopProgram();
    await fetch("/c2/api/program", { method: "DELETE" });
    await poll();
  }

  async function runProgram() {
    try {
      await fetch("/c2/api/stop", { method: "POST" });
    } catch (err) {
      /* onboard runner may already be idle */
    }
    const generated = generateProgram();
    document.getElementById("preview").textContent = JSON.stringify(generated.json, null, 2);
    rover.abort = new AbortController();
    document.getElementById("btnRun").disabled = true;
    document.getElementById("btnStop").disabled = false;
    const beat = setInterval(() => {
      rover.heartbeat().catch(() => {});
    }, 200);
    const poller = setInterval(() => {
      rover.refresh().then(renderHud).catch(() => {});
    }, 100);
    try {
      await rover.refresh();
      await rover.heartbeat();
      const runner = new Function("rover", "return (async () => {\n" + generated.code + "\n})();");
      await runner(rover);
    } catch (err) {
      if (!err || err.name !== "AbortError") {
        console.error(err);
      }
    } finally {
      clearInterval(beat);
      clearInterval(poller);
      try {
        await rover.stop();
      } catch (err) {
        /* ignore disconnect on stop */
      }
      rover.abort = null;
      renderHud();
    }
  }

  function stopProgram() {
    if (rover.abort) {
      rover.abort.abort();
    }
    rover.stop().catch(() => {}).then(poll);
  }

  function updateBlockHelp(block) {
    const help = block && BLOCK_HELP[block.type];
    const title = document.getElementById("blockHelpTitle");
    const body = document.getElementById("blockHelpBody");
    if (!title || !body) {
      return;
    }
    if (!help) {
      title.textContent = "Select a block";
      body.textContent = "Click a block on the workspace or in the toolbox. Sensor cheat sheet is below.";
      return;
    }
    title.textContent = help.title;
    body.textContent = help.body;
  }

  function helpFromEvent(workspaceRef, event) {
    const id = event.newElementId || event.blockId;
    if (id && workspaceRef && workspaceRef.getBlockById) {
      const found = workspaceRef.getBlockById(id);
      if (found) {
        updateBlockHelp(found);
        return;
      }
    }
    updateBlockHelp(Blockly.getSelected && Blockly.getSelected());
  }

  function isHelpEvent(event) {
    const selected = Blockly.Events && Blockly.Events.SELECTED;
    const click = Blockly.Events && Blockly.Events.CLICK;
    return event.type === selected || event.type === click ||
      event.type === "selected" || event.type === "click";
  }

  function collapseToolboxFlyout() {
    const toolbox = workspace.getToolbox && workspace.getToolbox();
    if (!toolbox) {
      return;
    }
    if (typeof toolbox.clearSelection === "function") {
      toolbox.clearSelection();
    }
    const flyout = toolbox.getFlyout && toolbox.getFlyout();
    if (flyout && typeof flyout.hide === "function") {
      flyout.hide();
    }
  }

  function placeAwayFromToolbox() {
    const toolbox = workspace.getToolbox && workspace.getToolbox();
    const width = toolbox && typeof toolbox.getWidth === "function" ? toolbox.getWidth() : 0;
    const pad = width + 24;
    workspace.getTopBlocks(false).forEach((block) => {
      const xy = block.getRelativeToSurfaceXY();
      if (xy.x < pad) {
        block.moveBy(pad - xy.x, 0);
      }
    });
  }

  function resizeWorkspace() {
    if (Blockly.svgResize) {
      Blockly.svgResize(workspace);
    }
  }

  function seedWorkspace() {
    const xml = `<xml xmlns="https://developers.google.com/blockly/xml">
      <block type="rover_on_start" x="16" y="16">
        <next>
          <block type="rover_move">
            <field name="DIR">forward</field>
            <field name="UNIT">ft</field>
            <value name="AMOUNT"><shadow type="rover_number"><field name="NUM">1</field></shadow></value>
            <next>
              <block type="rover_turn">
                <field name="DIR">right</field>
                <value name="DEG"><shadow type="rover_number"><field name="NUM">90</field></shadow></value>
              </block>
            </next>
          </block>
        </next>
      </block>
    </xml>`;
    if (Blockly.Xml && Blockly.utils && Blockly.utils.xml) {
      const dom = Blockly.utils.xml.textToDom(xml);
      Blockly.Xml.domToWorkspace(dom, workspace);
    }
  }

  defineBlocks();
  workspace = Blockly.inject("blocklyDiv", {
    toolbox: toolbox(),
    trashcan: true,
    zoom: { controls: true, wheel: true, startScale: 0.9 },
    grid: { spacing: 20, length: 3, colour: "#222", snap: true },
    move: { scrollbars: true, drag: true, wheel: true },
  });
  seedWorkspace();
  collapseToolboxFlyout();
  resizeWorkspace();
  placeAwayFromToolbox();
  window.addEventListener("resize", resizeWorkspace);
  workspace.addChangeListener((event) => {
    document.getElementById("preview").textContent =
      JSON.stringify(generateProgram().json, null, 2);
    if (isHelpEvent(event)) {
      helpFromEvent(workspace, event);
    }
  });
  const flyout = workspace.getFlyout && workspace.getFlyout();
  const flyoutWs = flyout && flyout.getWorkspace && flyout.getWorkspace();
  if (flyoutWs) {
    flyoutWs.addChangeListener((event) => {
      if (isHelpEvent(event)) {
        helpFromEvent(flyoutWs, event);
      }
    });
  }

  document.getElementById("btnRun").addEventListener("click", runProgram);
  document.getElementById("btnDeploy").addEventListener("click", deployProgram);
  const btnStop = document.getElementById("btnStop");
  const onStop = (event) => {
    event.preventDefault();
    event.stopPropagation();
    if (btnStop.disabled) {
      return;
    }
    stopProgram();
  };
  btnStop.addEventListener("pointerdown", onStop);
  btnStop.addEventListener("click", onStop);
  document.getElementById("btnUndeploy").addEventListener("click", undeployProgram);
  document.getElementById("btnResetMap").addEventListener("click", async () => {
    await fetch("/c2/api/_debug/pose", { method: "POST" });
    await poll();
  });
  document.getElementById("btnSaveSettings").addEventListener("click", async () => {
    const body = {};
    document.querySelectorAll("#settings input").forEach((input) => {
      body[input.name] = Number(input.value);
    });
    await fetch("/c2/api/settings", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    await poll();
  });

  window.addEventListener("pagehide", () => {
    if (rover.abort) {
      navigator.sendBeacon("/c2/api/stop", "");
    }
  });
  window.addEventListener("beforeunload", () => {
    if (rover.abort) {
      navigator.sendBeacon("/c2/api/stop", "");
    }
  });

  poll();
  setInterval(poll, 500);
})();
