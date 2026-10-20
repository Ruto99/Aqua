# このプロジェクトが使う RutoEngine の場所
# Aqua のリポジトリのサンプルなので、MyProgram/Aqua/Samples/AquaSamples から見た MyProgram/RutoEngine を使う
# （RutoLauncher で作ったプロジェクトは、ここに RutoEngine の場所がそのまま書かれる）
get_filename_component(RUTO_ENGINE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../../RutoEngine" ABSOLUTE)
