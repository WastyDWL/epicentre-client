Epicentre CS2-style HUD art

Source: "CS2 New Hud by SaViMoN" (a Counter-Strike: Source HUD), converted for the Epicentre client
by tools/import-cs2-hud.py: DXT5 VTF textures -> 32-bit TGA, fonts copied as they are.

  hud_bot.tga, hud_bot_tr.tga    bottom divider with the CT / T emblem
  ico_*.tga                      health, armor, armor+helmet, bullets, money, buy zone icons
  logo_ct.tga, logo_tr.tga       team logos
  name_*_team.tga, tabscore_bg   scoreboard art
  StratumNo2.ttf                 HUD digits
  CS2weapons.ttf                 weapon icon font

The art and fonts follow the look of Counter-Strike 2 and belong to their respective owners
(Valve Corporation, the font foundries and the HUD's author SaViMoN). client_mini loads every file
here as optional: delete the folder and the HUD falls back to its own drawn icons and Bahnschrift.
