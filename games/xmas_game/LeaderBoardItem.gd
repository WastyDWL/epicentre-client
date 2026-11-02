extends PanelContainer
class_name LeaderBoardItem

@onready var distance_stat: LeaderBoardStat = $Row / Stats / Distance
@onready var gifts_stat: LeaderBoardStat = $Row / Stats / Gifts
@onready var enemies_stat: LeaderBoardStat = $Row / Stats / Enemies
@onready var name_of_player_label: Label = $Row / Name
@onready var player_standing_label: Label = $Row / Standing
@onready var score_label: Label = $Row / Score

@export var distance_amount: int
@export var gifts_amount: int
@export var enemies_amount: int
@export var score_amount: int
@export var name_of_player: String = "Игрок"
@export var player_standing: int
@export var is_me: bool


func _ready() -> void :
	distance_stat.set_values(distance_amount)
	gifts_stat.set_values(gifts_amount)
	enemies_stat.set_values(enemies_amount)
	name_of_player_label.text = name_of_player
	player_standing_label.text = str(player_standing)
	score_label.text = str(score_amount)
	if is_me:
		name_of_player_label.add_theme_color_override("font_color", Color(0.984, 0.278, 0.294))


func set_entry(standing: int, entry: LeaderBoardModel, me: bool) -> void :
	player_standing = standing
	name_of_player = entry.name
	distance_amount = entry.distance
	gifts_amount = entry.gifts
	enemies_amount = entry.enemies
	score_amount = entry.score
	is_me = me
