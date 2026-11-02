extends VBoxContainer
class_name LeaderBoardStat

@export var stat_label_text: String
@export var stat_value_amount: int

@onready var stat_label: Label = $StatLabel
@onready var stat_value: Label = $StatValue


func _ready() -> void :
	stat_label.text = stat_label_text
	stat_value.text = str(stat_value_amount)


func set_values(new_stat_value: int) -> void :
	stat_value_amount = new_stat_value
	if is_node_ready():
		stat_value.text = str(new_stat_value)
