/*
 * GraphComponent.h
 * VoicemeeterHost — 節點圖畫布
 *
 * 此元件為插件節點、I/O 節點和連線（wire）的視覺化畫布。
 * 功能包含：
 *   - 節點拖曳定位（左/中/右三個 Zone 分區）
 *   - 連線拖曳（從輸出端口拉至輸入端口）
 *   - 縮放感知渲染（DPI 自適應，所有尺寸乘以 getDPIScaleFactor()）
 *   - 序列化/反序列化（以 XML 保存節點位置與連線狀態）
 */

#pragma once

#include <JuceHeader.h>
#include "NodeComponent.h"
#include "ConnectionComponent.h"

class GraphComponent : public juce::Component
{
public:
    // Zone 基準寬度 / 標頭高度（縮放前的邏輯像素值）
    static constexpr int kZoneW = 170; ///< 左/右 I/O Zone 的邏輯寬度
    static constexpr int kHdrH  = 34;  ///< Zone 標題列高度

    /** 取得 DPI 縮放後的 Zone 寬度（像素）。 */
    static int getZoneWidth()    { return static_cast<int> (kZoneW * getDPIScaleFactor()); }
    /** 取得 DPI 縮放後的標題列高度（像素）。 */
    static int getHeaderHeight() { return static_cast<int> (kHdrH  * getDPIScaleFactor()); }

    /** 音訊圖輸入 I/O 節點的固定 UID（對應 JUCE audioInputNode）。 */
    static constexpr juce::uint32 kInputNodeUID  = 1000000;
    /** 音訊圖輸出 I/O 節點的固定 UID（對應 JUCE audioOutputNode）。 */
    static constexpr juce::uint32 kOutputNodeUID = 1000001;

    /** 建構函式：注入音訊設備管理器、已知插件清單、格式管理器、以及音訊效果圖實例。 */
    GraphComponent (juce::AudioDeviceManager&      dm,
                    juce::KnownPluginList&          knownPlugins,
                    juce::AudioPluginFormatManager& fmt,
                    juce::AudioProcessorGraph&      graph);
    ~GraphComponent() override = default;

    /** 在圖中新增一個節點，指定顯示名稱與節點類型（Input/Output/Plugin）。 */
    void addNode (const juce::String& name, NodeType type);

    /** 取得所有節點的唯讀列表（PluginNode vector）。 */
    const std::vector<PluginNode>& getNodes() const noexcept { return nodes; }

    // ── Callbacks（外部設定的回調） ──
    /** 使用者請求管理（編輯）插件時觸發。 */
    std::function<void()>                      onManagePlugins;
    /** 在左側 I/O Zone 雙擊時觸發（通常開啟設備設定）。 */
    std::function<void()>                      onDoubleClickLeft;
    /** 在右側 I/O Zone 雙擊時觸發。 */
    std::function<void()>                      onDoubleClickRight;
    /** 使用者請求編輯指定節點（id, NodeType）時觸發。 */
    std::function<void (int, NodeType)>        onEditNode;
    /** 圖狀態發生變化（節點位置/連線/插件）時觸發，用於持久化。 */
    std::function<void()>                      onGraphChanged;

    // ── 序列化 ──
    /** 將目前圖狀態（節點清單、位置、連線）序列化為 XML 元素。 */
    std::unique_ptr<juce::XmlElement> saveState() const;
    /** 從 XML 元素還原圖狀態（清除現有節點並重建）。 */
    void loadState (const juce::XmlElement& xml);

    // ── Component 覆寫 ──
    /** 繪製畫布：背景、Zone 分區、節點與連線。 */
    void paint (juce::Graphics& g) override;
    /** 雙擊：在 Zone 中雙擊可開啟對應功能（設備設定或插件選取器）。 */
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    /** 按下：選取節點或開始拖曳連線。 */
    void mouseDown (const juce::MouseEvent& e) override;
    /** 拖曳：移動節點或更新連線尾端游標位置。 */
    void mouseDrag (const juce::MouseEvent& e) override;
    /** 放開：完成節點移動或嘗試建立連線。 */
    void mouseUp   (const juce::MouseEvent& e) override;
    /** 鍵盤：Delete/Backspace 鍵刪除所選節點。 */
    bool keyPressed (const juce::KeyPress& key) override;

private:
    // ── 注入的外部依賴（不擁有所有權） ──
    juce::AudioDeviceManager&       deviceManager;  ///< 音訊設備管理器（由 SystemTrayMenu 擁有）
    juce::KnownPluginList&          knownPlugins;   ///< 已掃描的 VST3 插件清單
    juce::AudioPluginFormatManager& formatManager;  ///< 插件格式管理器（用於實例化插件）
    juce::AudioProcessorGraph&      graph;          ///< 底層 JUCE 音訊處理效果圖

    // ── 資料模型 ──
    std::vector<PluginNode> nodes;   ///< 所有邏輯節點（含 I/O 與插件節點）
    std::vector<NodeWire>   wires;   ///< 所有連線（每條連線對應圖中一個 AudioGraph 連線）
    int nextId { 1 };                ///< 下一個可用的節點 ID（自增計數器）

    // ── 互動狀態 ──
    int        selectedNode { -1 };       ///< 目前選取的節點 ID（-1 表示無選取）
    int        draggingNode { -1 };       ///< 正在拖曳的節點 ID（-1 表示未拖曳節點）
    bool       draggingWire { false };    ///< 是否正在拖曳連線
    bool       wireDragFromInput { false }; ///< 拖曳連線時是否從輸入端口開始
    int        wireFrom     { -1 };       ///< 拖曳連線的起始節點 ID
    juce::Point<int> wireCursor;          ///< 拖曳連線時的游標位置

    // ── 幾何計算 ──
    /** Zone 分區枚舉：左側 I/O、中央插件區、右側 I/O。 */
    enum class Zone { Left, Center, Right };
    /** 根據畫布座標判斷屬於哪個 Zone。 */
    Zone                  zoneAt        (juce::Point<int> p) const;
    /** 計算節點的輸入端口（左側圓點）畫布座標。 */
    juce::Point<int>      inputPortPos  (const PluginNode& n) const;
    /** 計算節點的輸出端口（右側圓點）畫布座標。 */
    juce::Point<int>      outputPortPos (const PluginNode& n) const;
    /** 計算節點的邊界矩形（含 Zone 偏移與 DPI 縮放）。 */
    juce::Rectangle<int>  nodeBounds    (const PluginNode& n) const;

    // ── 繪製輔助 ──
    /** 繪製三個 Zone 的背景（含邊框、標題文字）。 */
    void drawZoneBackgrounds (juce::Graphics& g) const;

    // ── 命中測試 ──
    /** 找出給定點下方的節點 ID（-1 表示無）。 */
    int  nodeAtPoint    (juce::Point<int> p) const;
    /** 判斷給定點是否靠近某節點的輸出端口，若是則回傳節點 ID。 */
    bool nearOutputPort (juce::Point<int> p, int& outId) const;
    /** 判斷給定點是否靠近某節點的輸入端口，若是則回傳節點 ID。 */
    bool nearInputPort  (juce::Point<int> p, int& outId) const;
    /** 判斷從 fromId 到 toId 的連線是否合法（不能自接、不能重複、不能逆向）。 */
    bool isValidWire    (int fromId, int toId) const;

    // ── JUCE AudioProcessorGraph 連線管理 ──
    /** 在效果圖中建立從 from 節點到 to 節點的音訊連線。 */
    void addGraphConnection    (const PluginNode& from, const PluginNode& to);
    /** 移除效果圖中從 from 節點到 to 節點的音訊連線。 */
    void removeGraphConnection (const PluginNode& from, const PluginNode& to);
    /** 清除效果圖中所有連接到 to 節點輸入端的連線。 */
    void clearGraphInputConnections (const PluginNode& to);
    /** 斷開指定節點的所有連線（但不從 nodes 中移除）。 */
    void disconnectNode (int nodeId);

    // ── 插件互動 ──
    /** 在指定畫布位置顯示插件選取彈出選單（右鍵或雙擊中央 Zone）。 */
    void showPluginPicker (juce::Point<int> canvasPos);
    /** 開啟指定節點的插件編輯器視窗。 */
    void openPluginEditor (int nodeId);
    /** 從圖中移除指定節點（含效果圖節點與連線，並通知 onGraphChanged）。 */
    void removeNode (int nodeId);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphComponent)
};
