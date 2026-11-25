import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

const byId = (id) => document.getElementById(id);
const viewport = byId('viewport');
const status = byId('stage-status');
const scrub = byId('scrub');
const loop = byId('loop');
const propChoice = byId('prop');
const speed = byId('speed');
const play = byId('play');
const prev = byId('prev');
const next = byId('next');
const releaseMarker = byId('timeline-release');
const grabMarker = byId('timeline-grab');
const jumpRelease = byId('jump-release');
const clipButtons = [...document.querySelectorAll('[data-clip]')];
const cameraButtons = [...document.querySelectorAll('[data-camera]')];
const labels = {hold: 'Hold Object', throw: 'Throw Object'};
const controlsToLock = [scrub, play, prev, next, loop, speed, propChoice, jumpRelease, ...clipButtons, ...cameraButtons];
controlsToLock.forEach(button => { button.disabled = true; });

function color(value, roughness = .63, metalness = 0) {
  return new THREE.MeshStandardMaterial({ color: value, roughness, metalness });
}

function mesh(geometry, material, group, x = 0, y = 0, z = 0) {
  const object = new THREE.Mesh(geometry, material);
  object.position.set(x, y, z);
  object.castShadow = true;
  object.receiveShadow = true;
  group.add(object);
  return object;
}

function makeProps() {
  const stone = new THREE.Group();
  const stoneGeometry = new THREE.IcosahedronGeometry(.051, 1);
  const coordinates = stoneGeometry.attributes.position;
  for (let i = 0; i < coordinates.count; i++) {
    const x = coordinates.getX(i), y = coordinates.getY(i), z = coordinates.getZ(i);
    const ripple = 1 + .13 * Math.sin(53*x + 65*y + 39*z) * Math.cos(76*y - 60*z);
    coordinates.setXYZ(i, x*ripple, y*ripple*.91, z*ripple*.86);
  }
  stoneGeometry.computeVertexNormals();
  const rock = mesh(stoneGeometry, color(0xdb9b51, .88), stone);
  rock.rotation.set(.35, .32, .20);

  const crate = new THREE.Group();
  const wood = [color(0xb47b42), color(0xc58c50), color(0xad6b39)];
  for (let i = 0; i < 3; i++) {
    mesh(new THREE.BoxGeometry(.075, .024, .067), wood[i], crate, 0, (i-1)*.025, 0);
  }
  const band = color(0x5a422d);
  for (const x of [-.024, .024]) {
    mesh(new THREE.BoxGeometry(.009,.078,.069), band, crate, x, 0, 0);
    mesh(new THREE.BoxGeometry(.009,.080,.045), band, crate, x, 0, 0);
  }

  const flask = new THREE.Group();
  const glazed = color(0x5aafa2, .31, .10);
  const brass = color(0xbfa366, .36, .19);
  const clay = mesh(new THREE.SphereGeometry(.043, 28, 16), glazed, flask);
  clay.scale.set(.87, .95, .72);
  mesh(new THREE.CylinderGeometry(.015,.022,.034,20), glazed, flask, 0, .041, 0);
  mesh(new THREE.CylinderGeometry(.017,.017,.010,20), brass, flask, 0, .061, 0);
  mesh(new THREE.CylinderGeometry(.012,.012,.008,16), color(0xd7a573), flask, 0, .069, 0);
  flask.rotation.z = -.16;
  return {stone, crate, flask};
}

function addSoftContact(scene) {
  const source = document.createElement('canvas');
  source.width = source.height = 128;
  const context = source.getContext('2d');
  const ink = context.createRadialGradient(64,64,1,64,64,62);
  ink.addColorStop(0, 'rgba(1, 13, 15, 0.72)');
  ink.addColorStop(.47, 'rgba(1, 13, 15, 0.35)');
  ink.addColorStop(1, 'rgba(1, 13, 15, 0)');
  context.fillStyle = ink;
  context.fillRect(0, 0, 128, 128);
  const pad = new THREE.Mesh(new THREE.PlaneGeometry(1.95, 1.5), new THREE.MeshBasicMaterial({
    map: new THREE.CanvasTexture(source), transparent: true, depthWrite: false,
  }));
  pad.rotation.x = -Math.PI/2;
  pad.position.set(.1, -.036, .08);
  scene.add(pad);
  return pad;
}

async function start() {
  const [{scene: character, animations}, timing] = await Promise.all([
    new GLTFLoader().loadAsync(new URL('./assets/ThrowObject.glb', import.meta.url).href),
    fetch(new URL('./assets/timing.json', import.meta.url)).then((response) => {
      if (!response.ok) throw new Error('Unable to load the timing manifest.');
      return response.json();
    }),
  ]);
  const {fps, clips, release, pickup, attachment, locomotion} = timing;
  const glbClips = Object.fromEntries(Object.entries(clips).map(([key, spec]) => {
    const clip = animations.find(item => item.name === spec.name);
    if (!clip || Math.abs(clip.duration - spec.endFrame/fps) > .0005) {
      throw new Error('The GLB clips do not match the timing manifest.');
    }
    return [key, clip];
  }));
  const socket = character.getObjectByName(attachment.previewNode);
  if (!socket) throw new Error('Missing right-hand attachment socket.');

  const renderer = new THREE.WebGLRenderer({antialias: true, alpha: true});
  renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
  renderer.setClearColor(0, 0);
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1.2;
  renderer.shadowMap.enabled = true;
  renderer.shadowMap.type = THREE.PCFShadowMap;
  viewport.append(renderer.domElement);

  const scene = new THREE.Scene();
  const camera = new THREE.PerspectiveCamera(37, 1, .025, 36);
  const orbit = new OrbitControls(camera, renderer.domElement);
  orbit.enableDamping = true;
  orbit.minDistance = .24;
  orbit.maxDistance = 9;
  orbit.maxPolarAngle = Math.PI*.485;
  orbit.enablePan = false;
  const resize = () => {
    const {clientWidth: width, clientHeight: height} = viewport;
    renderer.setSize(width, height, false);
    camera.aspect = width / Math.max(1, height);
    camera.updateProjectionMatrix();
  };
  new ResizeObserver(resize).observe(viewport);
  resize();

  scene.add(character);
  character.traverse((object) => {
    if (object.isMesh) {
      object.castShadow = true;
      object.receiveShadow = true;
      object.frustumCulled = false;
    }
  });
  scene.add(new THREE.HemisphereLight(0xe6ffff, 0x617179, 2.18));
  const key = new THREE.DirectionalLight(0xffe8d1, 2.42);
  key.position.set(3.4, 5.5, -4.1);
  key.castShadow = true;
  key.shadow.mapSize.set(1024,1024);
  key.shadow.camera.left = key.shadow.camera.bottom = -2.4;
  key.shadow.camera.top = key.shadow.camera.right = 2.4;
  key.shadow.normalBias = .008;
  key.shadow.bias = -.0003;
  scene.add(key);
  const rim = new THREE.DirectionalLight(0x83eadf, 1.8);
  rim.position.set(-3, 2.2, 3.5);
  scene.add(rim);
  const floor = mesh(new THREE.PlaneGeometry(7.8,7.8), color(0x25454a, .95), scene, 0,-.045);
  floor.rotation.x = -Math.PI/2;
  floor.castShadow = false;
  const gridSpacing = .3;
  const grid = new THREE.GridHelper(7.8, 26, 0x51777d, 0x5c7d7e);
  grid.position.y = -.041;
  grid.material.opacity = .20;
  grid.material.transparent = true;
  scene.add(grid);
  const plinth = mesh(new THREE.CylinderGeometry(.88,.90,.035,100), color(0x426369, .82), scene,0,-.027,.10);
  plinth.castShadow = false;
  const ring = new THREE.Mesh(new THREE.RingGeometry(.82,.823,100), new THREE.MeshBasicMaterial({color:0x84aca9}));
  ring.rotation.x = -Math.PI/2;
  ring.position.set(0,-.008,.10);
  scene.add(ring);
  const contactPad = addSoftContact(scene);

  const mixer = new THREE.AnimationMixer(character);
  let action;
  const pivot = new THREE.Group();
  scene.add(pivot);
  const props = makeProps();
  for (const object of Object.values(props)) pivot.add(object);
  const launch = new THREE.Vector3();
  const launchRotation = new THREE.Quaternion();
  const velocity = new THREE.Vector3();
  const spin = new THREE.Quaternion();
  const floorHeight = .047;
  const spinAxis = new THREE.Vector3(.66,.38,.57).normalize();
  function runClip(key) {
    if (action) action.stop();
    action = mixer.clipAction(glbClips[key]);
    action.reset().setLoop(THREE.LoopOnce, 1).play();
    action.clampWhenFinished = true;
    action.paused = true;
  }
  function poseAt(seconds) {
    action.time = seconds;
    mixer.update(0);
    character.updateMatrixWorld(true);
  }
  runClip('throw');
  poseAt(release.seconds - 1/fps);
  const lastAttached = socket.getWorldPosition(new THREE.Vector3());
  poseAt(release.seconds);
  socket.getWorldPosition(launch);
  socket.getWorldQuaternion(launchRotation);
  velocity.copy(launch).sub(lastAttached).multiplyScalar(fps);
  const landingTime = (velocity.y + Math.sqrt(velocity.y**2 + 2*9.81*(launch.y-floorHeight)))/9.81;
  const bounceSpeed = (9.81*landingTime - velocity.y) * .24;
  runClip('pickup');
  poseAt(pickup.seconds);
  const restingPosition = socket.getWorldPosition(new THREE.Vector3());
  const restingRotation = socket.getWorldQuaternion(new THREE.Quaternion());
  pivot.position.copy(restingPosition);
  pivot.quaternion.copy(restingRotation);
  pivot.updateMatrixWorld(true);
  const probe = new THREE.Box3();
  const groundOffsets = Object.fromEntries(Object.entries(props).map(([name, object]) => [
    name, pickup.groundY-probe.setFromObject(object,true).min.y,
  ]));
  const cameraAnchor = new THREE.Vector3();
  const gripBefore = new THREE.Vector3();
  let cameraMode = 'orbit';
  let clipKey = 'hold';
  let time = 0;
  let cycles = 0;
  let running = false;
  let lastUIFrame = null;

  function setCamera(mode) {
    cameraMode = mode;
    cameraButtons.forEach(button => button.setAttribute('aria-pressed', String(button.dataset.camera === mode)));
    if (mode === 'grip') {
      socket.getWorldPosition(gripBefore);
      orbit.target.copy(gripBefore);
      camera.position.copy(gripBefore).add(new THREE.Vector3(.41,.21,-.40));
    } else if (mode === 'side') {
      orbit.target.set(.03,.91,-.20);
      camera.position.set(3.7,1.35,-.71);
    } else {
      orbit.target.set(.05,.93,-.16);
      camera.position.set(2.05,1.53,-2.94);
    }
    orbit.update();
  }

  function updatePlayButton() {
    play.setAttribute('aria-label', running ? 'Pause' : 'Play');
    play.title = running ? 'Pause (Space)' : 'Play (Space)';
    byId('play-shape').setAttribute('d', running ? 'M8 6h2v12H8z M14 6h2v12h-2z' : 'm9 5 11 7-11 7z');
  }

  function update() {
    poseAt(time);
    const spec = clips[clipKey];
    // The source stays in place. The floor moves backward at the same reference
    // speed used to author the foot contacts.
    const groundTravel = (spec.speedMps ?? 0)*time + (spec.travelM ?? 0)*cycles;
    grid.position.z = groundTravel % gridSpacing;
    const flying = clipKey === 'throw' && time >= release.seconds;
    const pickingUp = clipKey === 'pickup';
    const grounded = pickingUp && time < pickup.seconds;
    if (flying) {
      const flight = time - release.seconds;
      const airTime = Math.min(landingTime, flight);
      const afterLanding = flight - airTime;
      pivot.position.copy(launch).addScaledVector(velocity, airTime + afterLanding*.38);
      pivot.position.y = flight <= landingTime ?
        launch.y + velocity.y*flight - 4.905*flight*flight :
        floorHeight + Math.max(0, bounceSpeed*afterLanding - 4.905*afterLanding*afterLanding);
      spin.setFromAxisAngle(spinAxis, 7*airTime + 3*afterLanding);
      pivot.quaternion.copy(launchRotation).multiply(spin);
    } else if (grounded) {
      pivot.position.copy(restingPosition);
      pivot.quaternion.copy(restingRotation);
    } else {
      socket.getWorldPosition(pivot.position);
      socket.getWorldQuaternion(pivot.quaternion);
    }
    if (pickingUp) {
      const lift = THREE.MathUtils.smoothstep(time,pickup.seconds,pickup.seconds+4/fps);
      pivot.position.y += (groundOffsets[propChoice.value] ?? 0)*(1-lift);
    }
    // The prop meshes have their long axis in Three.js +Y. The socket is
    // already in the GLB hand's local coordinate system.
    for (const [key, object] of Object.entries(props)) object.visible = key === propChoice.value;
    if (cameraMode === 'grip') {
      socket.getWorldPosition(cameraAnchor);
      const delta = cameraAnchor.sub(gripBefore);
      orbit.target.add(delta);
      camera.position.add(delta);
      gripBefore.add(delta);
    }
    const frame = Math.min(clips[clipKey].endFrame, Math.floor(time * fps + 1e-7));
    if (frame !== lastUIFrame) {
      lastUIFrame = frame;
      scrub.value = String(frame);
      scrub.style.setProperty('--pos', `${frame/clips[clipKey].endFrame*100}%`);
      scrub.setAttribute('aria-valuetext', `Frame ${frame} of ${clips[clipKey].endFrame}`);
      byId('frame-number').value = String(frame).padStart(2, '0');
      byId('seconds').value = `${(frame/fps).toFixed(3)} s`;
    }
    let statusText = 'Right hand · Attached';
    if (propChoice.value === 'none') statusText = 'Placeholder hidden';
    else if (grounded) statusText = 'On ground';
    else if (flying) statusText = 'Right hand · Released';
    if (byId('prop-status').textContent !== statusText) byId('prop-status').textContent = statusText;
    byId('scene-label').parentElement.classList.toggle('released', flying);
  }

  function selectClip(key, frame = 0, afterPickup = false) {
    clipKey = key;
    running = false;
    lastUIFrame = null;
    runClip(key);
    const spec = clips[key];
    const isCarrying = spec.travelM !== undefined;
    plinth.visible = ring.visible = contactPad.visible = !isCarrying;
    const usePickupGround = key === 'pickup' || afterPickup;
    plinth.position.y = usePickupGround ? pickup.groundY-.0175 : -.027;
    ring.position.y = usePickupGround ? pickup.groundY+.0015 : -.008;
    floor.position.y = isCarrying ? locomotion.groundY : -.045;
    grid.position.y = floor.position.y+.004;
    cycles = 0;
    loop.checked = spec.loop;
    scrub.max = String(spec.endFrame);
    byId('frame-total').textContent = `/ ${spec.endFrame}`;
    byId('scene-label').textContent = spec.label ?? labels[key];
    const ticks = Array.from({length:5}, (_,i) => Math.round(i*spec.endFrame/4));
    byId('ticks').replaceChildren(...ticks.map(tick => {
      const label = document.createElement('span');
      label.textContent = String(tick).padStart(2,'0');
      return label;
    }));
    const placeMarker = (marker, frame, visible) => {
      const fraction = frame / spec.endFrame;
      marker.hidden = !visible;
      marker.style.left = `calc(${fraction*100}% + ${(0.5-fraction)*13}px)`;
    };
    placeMarker(releaseMarker,release.frame,key === 'throw');
    placeMarker(grabMarker,pickup.frame,key === 'pickup');
    clipButtons.forEach(button => button.setAttribute('aria-pressed', String(button.dataset.clip === key)));
    time = frame / fps;
    updatePlayButton();
    update();
  }

  function step(amount) {
    running = false;
    let frame = lastUIFrame + amount;
    const end = clips[clipKey].endFrame;
    if (loop.checked) frame = (frame + end + 1) % (end + 1);
    time = THREE.MathUtils.clamp(frame, 0, end) / fps;
    updatePlayButton();
    update();
  }
  function toggle() {
    running = !running;
    if (running && time >= clips[clipKey].endFrame / fps) {
      time = 0;
      cycles = 0;
    }
    updatePlayButton();
    update();
  }
  const jump = () => selectClip('throw', release.frame);
  jumpRelease.addEventListener('click', jump);
  releaseMarker.addEventListener('click', jump);
  grabMarker.addEventListener('click', () => selectClip('pickup',pickup.frame));
  play.addEventListener('click', toggle);
  prev.addEventListener('click', () => step(-1));
  next.addEventListener('click', () => step(1));
  scrub.addEventListener('input', () => {
    running = false;
    time = Number(scrub.value)/fps;
    cycles = 0;
    updatePlayButton();
    update();
  });
  propChoice.addEventListener('change', update);
  clipButtons.forEach(button => button.addEventListener('click', () => selectClip(button.dataset.clip)));
  cameraButtons.forEach(button => button.addEventListener('click', () => setCamera(button.dataset.camera)));
  document.addEventListener('keydown', (event) => {
    if (event.target.closest('button, a, input, select, textarea') || event.altKey || event.ctrlKey || event.metaKey) return;
    const shortcut = {Space: toggle, ArrowLeft: () => step(-1), ArrowRight: () => step(1)}[event.code];
    if (shortcut) {
      event.preventDefault();
      shortcut();
    }
  });

  selectClip('hold');
  setCamera('orbit');
  controlsToLock.forEach(button => { button.disabled = false; });
  status.hidden = true;
  let previous = performance.now();
  renderer.setAnimationLoop((now) => {
    const delta = Math.min(.1, Math.max(0, (now-previous)/1000));
    previous = now;
    if (running) {
      time += delta * Number(speed.value);
      const end = clips[clipKey].endFrame/fps;
      if (time >= end) {
        if (loop.checked) {
          cycles += Math.floor(time/end);
          time %= end;
        } else if (clips[clipKey].next) {
          selectClip(clips[clipKey].next,(time-end)*fps,true);
          running = true;
          updatePlayButton();
        } else {
          time = end;
          running = false;
          updatePlayButton();
        }
      }
      update();
    }
    orbit.update();
    renderer.render(scene, camera);
  });
}

start().catch((error) => {
  console.error(error);
  status.textContent = 'The 3D preview could not start. Serve the project over HTTP in a WebGL-enabled browser.';
});
