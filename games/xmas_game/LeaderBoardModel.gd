extends RefCounted
class_name LeaderBoardModel

var name: String
var distance: int
var gifts: int
var crates: int
var enemies: int
var score: int


static func from_dict(data: Dictionary) -> LeaderBoardModel:
	var entry: = LeaderBoardModel.new()
	entry.name = str(data.get("name", ""))
	entry.distance = int(data.get("distance", 0))
	entry.gifts = int(data.get("gifts", 0))
	entry.crates = int(data.get("crates", 0))
	entry.enemies = int(data.get("enemies", 0))
	entry.score = int(data.get("score", 0))
	return entry
