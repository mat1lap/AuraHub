#!/bin/bash
set -e # Остановить выполнение при ошибке

# Я сидел мучался на кэшиОС с билдом потому что симэйк коряво подтягивает все зависимости и ничё не работает
# Поэтому вот этот скрипт и появился

BUILD_DIR="build_userver2"
PATCH_PATH="cmake/patches/userver_v2.14_aura_yaml_scalar_fixes.patch"

echo "--- 📦 Проверка и установка системных зависимостей (Arch/CachyOS) ---"
echo "Устанавливаю пакеты из официальных репозиториев..."
sudo pacman -S --needed --noconfirm base-devel pkgconf postgresql-libs boost boost-libs \
    fmt c-ares yaml-cpp re2 libev zstd crypto++ jemalloc libnghttp2 curl iptables-nft

# Проверка и установка cctz из AUR
if ! pacman -Qs cctz > /dev/null ; then
    echo "Устанавливаю cctz из AUR..."
    if command -v paru &> /dev/null; then
        paru -S --noconfirm --needed cctz
    elif command -v yay &> /dev/null; then
        yay -S --noconfirm --needed cctz
    else
        echo "⚠️ Внимание: Не найден AUR-хелпер (paru или yay). Сборка может упасть без cctz."
    fi
fi

echo "--- 🧹 Очистка старого билда ---"
# Очищаем папку сборки, чтобы CMake подхватил системные пакеты, а не пытался качать их сам
rm -rf "$BUILD_DIR"

echo "--- 🛠 Исправление зависимостей ---"
# Автоматически добавляем nlohmann_json в server/CMakeLists.txt, если его там нет
if [ -f "server/CMakeLists.txt" ] && ! grep -q "nlohmann_json::nlohmann_json" server/CMakeLists.txt; then
    echo "Добавляю nlohmann_json в server/CMakeLists.txt..."
    sed -i '/target_link_libraries(auracloud/a \    nlohmann_json::nlohmann_json' server/CMakeLists.txt
fi

echo "--- ⚙️ Конфигурация CMake ---"
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DAURAHUB_BUILD_USERVER=ON \
    -DAURAHUB_BUILD_CLIENT=OFF \
    -DAURAHUB_BUILD_TESTS=OFF

echo "--- 🩹 Применение патчей (userver) ---"
# Проверяем наличие патча и применяем его к исходникам userver
if [ -f "$PATCH_PATH" ]; then
    USERVER_SRC="$BUILD_DIR/_deps/userver-src"
    if [ -d "$USERVER_SRC" ]; then
        echo "Применяю патч для корректного парсинга YAML..."
        patch -p1 -d "$USERVER_SRC" < "$PATCH_PATH" || echo "Патч уже применен или не требуется."
    fi
fi

echo "--- 🏗 Сборка сервера ---"
cmake --build "$BUILD_DIR" --target auracloud_userver -j$(nproc)

echo "--- ✅ Билд завершен! ---"
echo "Теперь можно запускать ./scripts/demo_agent_approval.sh"