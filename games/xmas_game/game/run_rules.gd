class_name RunRules



const GRAVITY: = 32.0
const JUMP_VELOCITY: = 12.0
const DOUBLE_JUMP_VELOCITY: = 10.5
const SPEED_MIN: = 7.0
const SPEED_MAX: = 14.0
const SPEED_RAMP_SECONDS: = 180.0
const LIVES: = 3
const INVULNERABLE: = 1.5
const SNOWBALL_COOLDOWN: = 0.35
const COMBO_STEP: = 5
const COMBO_MAX: = 5

const SAFETY: = 0.75

const POINTS: = {
	&"gift": 10, 
	&"crate": 5, 
	&"snowman": 25, 
	&"snowman_stomp": 35, 
	&"crow": 40, 
}


static func speed_at(t: float) -> float:
	return lerpf(SPEED_MIN, SPEED_MAX, clampf(t / SPEED_RAMP_SECONDS, 0.0, 1.0))


static func double_jump_height() -> float:
	return (JUMP_VELOCITY * JUMP_VELOCITY + DOUBLE_JUMP_VELOCITY * DOUBLE_JUMP_VELOCITY) / (2.0 * GRAVITY)



static func air_time() -> float:
	var up: = JUMP_VELOCITY / GRAVITY + DOUBLE_JUMP_VELOCITY / GRAVITY
	var down: = sqrt(2.0 * double_jump_height() / GRAVITY)
	return up + down


static func max_gap(speed: float) -> float:
	return speed * air_time() * SAFETY


static func max_step_up() -> float:
	return double_jump_height() * SAFETY


static func points(kind: StringName) -> int:
	return POINTS.get(kind, 0)


static func multiplier(actions_without_damage: int) -> int:
	return mini(1 + actions_without_damage / COMBO_STEP, COMBO_MAX)
