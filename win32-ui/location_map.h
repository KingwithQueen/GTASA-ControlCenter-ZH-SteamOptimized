/* Original coordinate visualization for the legacy saved locations. */
#ifndef GTASA_LOCATION_MAP_H
#define GTASA_LOCATION_MAP_H

#include <windowsx.h>

typedef struct {
    HWND window, zoom, label;
    HFONT font;
    HBITMAP background;
    SIZE bufferSize;
    int zoomIndex, markerSize, visibleCount;
    const int *visible;
    POINT center, origin, dragStart, manual, player;
    BOOL dirty, showMarkers, dragging, moved, hasManual, hasPlayer;
} LocationMap;

static LocationMap mapView;
static const WCHAR mapClass[] = L"GTASAControlCenterLocationMap";
static const int mapZoomLevels[] = { 50, 75, 100, 150, 200, 300, 400 };

typedef struct {
    LPCWSTR name;
    COLORREF color;
    POINT caption;
    int count;
    POINT outline[9];
} MapRegion;

static const MapRegion mappedRegions[] = {
    { L"圣辉洛", RGB(212, 228, 225), { -2320, 360 }, 6,
      { { -2950, -150 }, { -2780, 1320 }, { -1870, 1340 }, { -1510, 860 },
        { -1510, -390 }, { -2600, -690 } } },
    { L"骨郡与沙漠", RGB(239, 221, 182), { -450, 2180 }, 7,
      { { -1770, 1200 }, { -1090, 2710 }, { 150, 2890 }, { 1040, 2680 },
        { 1070, 930 }, { 250, 780 }, { -1120, 390 } } },
    { L"拉斯文加斯", RGB(246, 235, 206), { 2040, 2220 }, 6,
      { { 1170, 950 }, { 1160, 2765 }, { 2510, 2850 }, { 2870, 2250 },
        { 2840, 885 }, { 2140, 690 } } },
    { L"乡郊地区", RGB(213, 233, 204), { -1090, -1240 }, 8,
      { { -2660, -850 }, { -1490, -600 }, { -1190, 200 }, { 360, 610 },
        { 1530, 420 }, { 1480, -400 }, { 850, -1260 }, { -1080, -2400 } } },
    { L"洛圣都", RGB(207, 228, 214), { 1690, -2440 }, 7,
      { { 350, -2460 }, { 500, -1520 }, { 1110, -1060 }, { 2150, -855 },
        { 2855, -1110 }, { 2920, -2480 }, { 1840, -2870 } } }
};

static void SelectLocationFromMap(int row);

static int MapPixels(const RECT *client)
{
    int available = min(client->right - client->left, client->bottom - client->top);
    return max(1, MulDiv(available - 18, mapZoomLevels[mapView.zoomIndex], 100));
}

static POINT MapScreenPoint(const RECT *client, POINT coordinate)
{
    POINT pixel;
    int side = MapPixels(client);
    pixel.x = (client->left + client->right) / 2 + MulDiv(coordinate.x - mapView.center.x, side, 6000);
    pixel.y = (client->top + client->bottom) / 2 - MulDiv(coordinate.y - mapView.center.y, side, 6000);
    return pixel;
}

static POINT MapWorldPoint(const RECT *client, POINT pixel)
{
    POINT coordinate;
    int side = MapPixels(client);
    coordinate.x = mapView.center.x + MulDiv(pixel.x - (client->left + client->right) / 2, 6000, side);
    coordinate.y = mapView.center.y - MulDiv(pixel.y - (client->top + client->bottom) / 2, 6000, side);
    return coordinate;
}

static void CenterLocationMap(POINT point)
{
    mapView.center.x = max(-3000, min(3000, point.x));
    mapView.center.y = max(-3000, min(3000, point.y));
    mapView.dirty = TRUE;
    InvalidateRect(mapView.window, NULL, FALSE);
}

static void SetMapZoom(int index, const POINT *anchor)
{
    POINT previousWorld = { 0, 0 }, nextWorld, center;
    RECT client;
    index = max(0, min((int)ARRAYSIZE(mapZoomLevels) - 1, index));
    if (index == mapView.zoomIndex)
        return;
    GetClientRect(mapView.window, &client);
    center = mapView.center;
    if (anchor)
        previousWorld = MapWorldPoint(&client, *anchor);
    mapView.zoomIndex = index;
    SendMessageW(mapView.zoom, CB_SETCURSEL, index, 0);
    if (anchor) {
        nextWorld = MapWorldPoint(&client, *anchor);
        center.x += previousWorld.x - nextWorld.x;
        center.y += previousWorld.y - nextWorld.y;
    }
    CenterLocationMap(center);
}

static void SetManualPoint(POINT point, BOOL center)
{
    WCHAR description[100];
    mapView.manual = point;
    mapView.hasManual = TRUE;
    StringCchPrintfW(description, ARRAYSIZE(description), L"地图标记  X %ld  Y %ld", point.x, point.y);
    SetWindowTextW(mapView.label, description);
    if (center)
        CenterLocationMap(point);
    else
        InvalidateRect(mapView.window, NULL, FALSE);
}

static int NearbyMapRecord(const RECT *client, POINT pixel)
{
    const RecordSet *set = RecordsFor(PAGE_LOCATIONS);
    const Record *row;
    POINT marker;
    int index, candidate = -1;
    LONG dx, dy, distance = 13 * 13;
    if (!mapView.showMarkers)
        return -1;
    for (index = 0; index < mapView.visibleCount; ++index) {
        row = &set->rows[mapView.visible[index]];
        if (!row->validPoint)
            continue;
        marker = MapScreenPoint(client, row->point);
        dx = marker.x - pixel.x;
        dy = marker.y - pixel.y;
        if (dx * dx + dy * dy < distance) {
            distance = dx * dx + dy * dy;
            candidate = index;
        }
    }
    return candidate;
}

static void DrawMarker(HDC dc, POINT point, int size, HBRUSH brush)
{
    RECT box;
    SetRect(&box, point.x - size, point.y - size, point.x + size + 1, point.y + size + 1);
    FillRect(dc, &box, brush);
}

static void DrawMapBackground(HDC dc, const RECT *bounds)
{
    const RecordSet *set = RecordsFor(PAGE_LOCATIONS);
    POINT points[9], world, from, to;
    HBRUSH brush;
    HPEN outline, grid, route;
    HGDIOBJ previousBrush, previousPen;
    int index, point, i;
    WCHAR axis[24];
    unsigned region;
    RECT label;

    brush = CreateSolidBrush(RGB(216, 235, 246));
    FillRect(dc, bounds, brush ? brush : GetSysColorBrush(COLOR_WINDOW));
    if (brush)
        DeleteObject(brush);

    outline = CreatePen(PS_SOLID, 1, RGB(119, 154, 158));
    previousPen = SelectObject(dc, outline ? outline : GetStockObject(BLACK_PEN));
    for (region = 0; region < ARRAYSIZE(mappedRegions); ++region) {
        const MapRegion *area = &mappedRegions[region];
        brush = CreateSolidBrush(area->color);
        previousBrush = SelectObject(dc, brush ? brush : GetSysColorBrush(COLOR_WINDOW));
        for (point = 0; point < area->count; ++point)
            points[point] = MapScreenPoint(bounds, area->outline[point]);
        Polygon(dc, points, area->count);
        SelectObject(dc, previousBrush);
        if (brush)
            DeleteObject(brush);
    }
    SelectObject(dc, previousPen);
    if (outline)
        DeleteObject(outline);

    grid = CreatePen(PS_DOT, 1, RGB(132, 166, 181));
    previousPen = SelectObject(dc, grid ? grid : GetStockObject(BLACK_PEN));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(53, 87, 104));
    if (mapView.font)
        SelectObject(dc, mapView.font);
    for (index = -3000; index <= 3000; index += 1000) {
        world.x = index;
        world.y = -3000;
        from = MapScreenPoint(bounds, world);
        world.y = 3000;
        to = MapScreenPoint(bounds, world);
        MoveToEx(dc, from.x, from.y, NULL);
        LineTo(dc, to.x, to.y);
        if (index) {
            StringCchPrintfW(axis, ARRAYSIZE(axis), L"%d", index);
            TextOutW(dc, from.x + 3, max(bounds->top + 4, to.y + 18), axis, (int)wcslen(axis));
        }
        world.x = -3000;
        world.y = index;
        from = MapScreenPoint(bounds, world);
        world.x = 3000;
        to = MapScreenPoint(bounds, world);
        MoveToEx(dc, from.x, from.y, NULL);
        LineTo(dc, to.x, to.y);
        StringCchPrintfW(axis, ARRAYSIZE(axis), L"%d", index);
        TextOutW(dc, max(bounds->left + 3, from.x + 3), from.y + 3, axis, (int)wcslen(axis));
    }
    SelectObject(dc, previousPen);
    if (grid)
        DeleteObject(grid);

    route = CreatePen(PS_DASH, 1, RGB(145, 132, 90));
    previousPen = SelectObject(dc, route ? route : GetStockObject(BLACK_PEN));
    for (i = 0; i < (int)ARRAYSIZE(mappedRegions) - 1; ++i) {
        from = MapScreenPoint(bounds, mappedRegions[i].caption);
        to = MapScreenPoint(bounds, mappedRegions[i + 1].caption);
        MoveToEx(dc, from.x, from.y, NULL);
        LineTo(dc, to.x, to.y);
    }
    SelectObject(dc, previousPen);
    if (route)
        DeleteObject(route);

    if (mapView.showMarkers) {
        brush = CreateSolidBrush(RGB(57, 99, 129));
        if (brush) {
            for (index = 0; index < mapView.visibleCount; ++index) {
                const Record *entry = &set->rows[mapView.visible[index]];
                if (entry->validPoint) {
                    from = MapScreenPoint(bounds, entry->point);
                    if (PtInRect(bounds, from))
                        DrawMarker(dc, from, max(1, (mapView.markerSize - 1) / 2), brush);
                }
            }
            DeleteObject(brush);
        }
    }
    SetTextColor(dc, RGB(30, 64, 80));
    for (region = 0; region < ARRAYSIZE(mappedRegions); ++region) {
        from = MapScreenPoint(bounds, mappedRegions[region].caption);
        SetRect(&label, from.x - 58, from.y - 9, from.x + 58, from.y + 10);
        DrawTextW(dc, mappedRegions[region].name, -1, &label, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    SetRect(&label, bounds->left, bounds->top + 7, bounds->right - 16, bounds->bottom);
    DrawTextW(dc, L"北↑", 2, &label, DT_RIGHT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    FrameRect(dc, bounds, GetSysColorBrush(COLOR_3DSHADOW));
}

static void DrawMapHighlights(HDC dc, const RECT *bounds)
{
    const RecordSet *set = RecordsFor(PAGE_LOCATIONS);
    const Record *row;
    HWND list = GetDlgItem(GetParent(mapView.window), 1000);
    int selected = ListView_GetNextItem(list, -1, LVNI_SELECTED);
    POINT point;
    HBRUSH brush;
    if (selected >= 0 && selected < mapView.visibleCount) {
        row = &set->rows[mapView.visible[selected]];
        if (row->validPoint && mapView.showMarkers) {
            point = MapScreenPoint(bounds, row->point);
            DrawMarker(dc, point, 6, GetSysColorBrush(COLOR_WINDOW));
            brush = CreateSolidBrush(RGB(210, 136, 15));
            if (brush) {
                DrawMarker(dc, point, 4, brush);
                DeleteObject(brush);
            }
        }
    }
    if (mapView.hasManual) {
        point = MapScreenPoint(bounds, mapView.manual);
        DrawMarker(dc, point, 6, GetSysColorBrush(COLOR_WINDOW));
        DrawMarker(dc, point, 4, GetSysColorBrush(COLOR_HIGHLIGHT));
    }
    if (mapView.hasPlayer) {
        point = MapScreenPoint(bounds, mapView.player);
        brush = CreateSolidBrush(RGB(188, 49, 61));
        if (brush) {
            DrawMarker(dc, point, 3, brush);
            DeleteObject(brush);
        }
    }
}

static void PaintLocationMap(HWND window)
{
    PAINTSTRUCT paint;
    RECT client;
    HDC dc = BeginPaint(window, &paint);
    HDC buffer = CreateCompatibleDC(dc);
    HGDIOBJ previous;
    GetClientRect(window, &client);
    if (mapView.bufferSize.cx != client.right || mapView.bufferSize.cy != client.bottom) {
        if (mapView.background)
            DeleteObject(mapView.background);
        mapView.background = CreateCompatibleBitmap(dc, client.right, client.bottom);
        mapView.bufferSize.cx = client.right;
        mapView.bufferSize.cy = client.bottom;
        mapView.dirty = TRUE;
    }
    if (buffer && mapView.background) {
        previous = SelectObject(buffer, mapView.background);
        if (mapView.dirty) {
            DrawMapBackground(buffer, &client);
            mapView.dirty = FALSE;
        }
        BitBlt(dc, 0, 0, client.right, client.bottom, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, previous);
    } else {
        DrawMapBackground(dc, &client);
    }
    if (buffer)
        DeleteDC(buffer);
    DrawMapHighlights(dc, &client);
    if (GetFocus() == window) {
        InflateRect(&client, -2, -2);
        DrawFocusRect(dc, &client);
    }
    EndPaint(window, &paint);
}

static LRESULT CALLBACK LocationMapProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    POINT pixel, coordinate, center;
    RECT bounds;
    int index, side;
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        PaintLocationMap(window);
        return 0;
    case WM_GETDLGCODE:
        return DLGC_WANTARROWS;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(window, NULL, FALSE);
        return 0;
    case WM_MOUSEWHEEL:
        pixel.x = GET_X_LPARAM(lParam);
        pixel.y = GET_Y_LPARAM(lParam);
        ScreenToClient(window, &pixel);
        if (GET_WHEEL_DELTA_WPARAM(wParam))
            SetMapZoom(mapView.zoomIndex + (GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 1 : -1), &pixel);
        return 0;
    case WM_LBUTTONDOWN:
        SetFocus(window);
        SetCapture(window);
        mapView.dragging = TRUE;
        mapView.moved = FALSE;
        mapView.origin = mapView.center;
        mapView.dragStart.x = GET_X_LPARAM(lParam);
        mapView.dragStart.y = GET_Y_LPARAM(lParam);
        return 0;
    case WM_MOUSEMOVE:
        pixel.x = GET_X_LPARAM(lParam);
        pixel.y = GET_Y_LPARAM(lParam);
        if (mapView.dragging) {
            if (mapView.moved || abs(pixel.x - mapView.dragStart.x) + abs(pixel.y - mapView.dragStart.y) > 5) {
                GetClientRect(window, &bounds);
                side = MapPixels(&bounds);
                center.x = mapView.origin.x - MulDiv(pixel.x - mapView.dragStart.x, 6000, side);
                center.y = mapView.origin.y + MulDiv(pixel.y - mapView.dragStart.y, 6000, side);
                mapView.moved = TRUE;
                CenterLocationMap(center);
            }
        } else {
            GetClientRect(window, &bounds);
            index = NearbyMapRecord(&bounds, pixel);
            if (index >= 0)
                SetWindowTextW(mapView.label, RecordsFor(PAGE_LOCATIONS)->rows[mapView.visible[index]].field[0]);
        }
        return 0;
    case WM_LBUTTONUP:
        if (mapView.dragging) {
            mapView.dragging = FALSE;
            ReleaseCapture();
            if (!mapView.moved) {
                pixel.x = GET_X_LPARAM(lParam);
                pixel.y = GET_Y_LPARAM(lParam);
                GetClientRect(window, &bounds);
                coordinate = MapWorldPoint(&bounds, pixel);
                if (abs(coordinate.x) <= 3000 && abs(coordinate.y) <= 3000)
                    SetManualPoint(coordinate, FALSE);
            }
        }
        return 0;
    case WM_LBUTTONDBLCLK:
        pixel.x = GET_X_LPARAM(lParam);
        pixel.y = GET_Y_LPARAM(lParam);
        GetClientRect(window, &bounds);
        index = NearbyMapRecord(&bounds, pixel);
        if (index >= 0)
            SelectLocationFromMap(index);
        return 0;
    case WM_CAPTURECHANGED:
        mapView.dragging = FALSE;
        return 0;
    case WM_KEYDOWN:
        center = mapView.center;
        if (wParam == VK_LEFT)
            center.x -= 100;
        else if (wParam == VK_RIGHT)
            center.x += 100;
        else if (wParam == VK_UP)
            center.y += 100;
        else if (wParam == VK_DOWN)
            center.y -= 100;
        else
            break;
        CenterLocationMap(center);
        return 0;
    case WM_NCDESTROY:
        if (mapView.background)
            DeleteObject(mapView.background);
        mapView.background = NULL;
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

static void RegisterLocationMap(UiPage *page)
{
    WNDCLASSW settings = { 0 };
    settings.style = CS_DBLCLKS;
    settings.lpfnWndProc = LocationMapProc;
    settings.hInstance = GetModuleHandleW(NULL);
    settings.hCursor = LoadCursorW(NULL, IDC_CROSS);
    settings.lpszClassName = mapClass;
    if (!RegisterClassW(&settings) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        page->failed = TRUE;
}

#endif
