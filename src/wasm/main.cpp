#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <emscripten.h>
#include <random>
#include <vector>

using namespace std;

constexpr double PI = 3.14159265358979323846;

EM_JS(void, render_canvas, (const uint8_t *data, int size), {
  const pixels = HEAPU8.subarray(data, data + size);
  self.postMessage({action : 'update_frame', data : pixels});
});

EM_JS(void, debug_state, (int frame_time), {
  const data = {frame_time};
  self.postMessage({action : 'debug', data : data});
});

// add measuments use screen height as reference
#define PADDLE_HEIGHT_PERC 0.3 // 30%
#define PADDLE_VELOCITY 0.05   // 5% / tick
#define PADDLE_WIDTH_PERC 0.05
#define PADDLE_WIDTH_OFFSET_PERC 0.05
#define BALL_SIZE_PERC 0.05 // radis is 5% of the height

#define MAX_BALL_VELOCITY 0.1 // 0.5% of the screen height

#define FRAME_BUDGET 0.033333333 // 60 fps

enum Shape {
  LINE,   // length
  RECT,   // height x width
  CIRCLE, // radius
};

struct dimension2D {
  int _1;
  int _2;
  Shape shape;
};

struct motion2D {
  double velocity;
  double angle;
};

struct position2D {
  int x;
  int y;
};

// make this a circle eventually
struct Ball {
  motion2D motion;
  dimension2D dim;
  position2D position;
  bool hasCollision;
};

struct Paddle {
  motion2D motion;
  position2D position;
  dimension2D dim;
  bool hasCollision;
};

struct Line {
  position2D start;
  position2D end;
  dimension2D dim;
  bool hasCollision;
};

struct CanvasPixel {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
};
static const CanvasPixel WHITE_PIXEL = {255, 255, 255, 255};
static const CanvasPixel BLACK_PIXEL = {0, 0, 0, 255};

struct Frame {
  int width;
  int height;
  vector<CanvasPixel> data;

  void construct(int _width, int _height) {
    width = _width;
    height = _height;
    wipe();
  }

  void wipe() { data.assign(width * height, BLACK_PIXEL); }

  bool putPixel(int x, int y, CanvasPixel px) {
    if (x >= width || y >= height) {
      return false;
    }

    int pos = (y * width) + x;
    data[pos].r = px.r;
    data[pos].g = px.g;
    data[pos].b = px.b;
    data[pos].a = px.a;

    return true;
  }

  void render() {
    vector<uint8_t> rawFrame = flatten();
    int frame_size = static_cast<int>(rawFrame.size());
    render_canvas(rawFrame.data(), frame_size);
  }

  void draw_line(dimension2D dim, position2D start, position2D end) {
    if (dim.shape != LINE) {
      return;
    }

    int diff_x = start.x - end.x;
    int diff_y = start.y - end.y;

    if (diff_x != 0 && diff_y != 0) {
      printf("diagonal lines are not yet supported");
      return;
    }

    // vertical line
    if (diff_x == 0) {
      int max_y = 0, min_y = 0;
      if (start.y > end.y) {
        max_y = start.y;
        min_y = end.y;
      } else {
        max_y = end.y;
        min_y = start.y;
      }

      for (int i = min_y; i <= max_y; i++) {
        putPixel(start.x, i, WHITE_PIXEL);
      }
      return;
    }

    // vertical line
    if (diff_y == 0) {
      int max_x = 0, min_x = 0;
      if (start.x > end.x) {
        max_x = start.x;
        min_x = end.x;
      } else {
        max_x = end.x;
        min_x = start.x;
      }

      for (int i = min_x; i <= max_x; i++) {
        putPixel(i, start.y, WHITE_PIXEL);
      }
      return;
    }
  }

  void draw_rect(dimension2D dim, position2D pos) {
    if (dim.shape != RECT)
      return;

    int x0 = pos.x - dim._2 / 2;
    if (x0 < 0) {
      x0 = 0;
    }

    int y0 = pos.y - dim._1 / 2;
    if (y0 < 0) {
      y0 = 0;
    }

    vector<CanvasPixel> row;
    row.reserve(dim._2);

    int iter = 0;
    int row_count = 0;

    for (int i = (width * y0) + x0; i <= width * height;) {
      if (iter == dim._2) {
        std::copy(row.begin(), row.end(),
                  data.begin() + (width * (y0 + row_count)) + x0);

        row.clear();
        iter = 0;
        row_count++;

        if (row_count == dim._1) {
          break;
        }

        i = (width * (y0 + row_count)) + x0;
        continue;
      }

      row.push_back(WHITE_PIXEL);
      iter++;
      i++;
    }
  }

  vector<uint8_t> flatten() {
    vector<uint8_t> image;
    image.reserve(data.size() * 4);

    for (const CanvasPixel &px : data) {
      image.push_back(px.r);
      image.push_back(px.g);
      image.push_back(px.b);
      image.push_back(px.a);
    }

    return image;
  }
};

struct Field {
  int width;
  int height;

  Line *left_wall;
  Line *top_wall;
  Line *right_wall;
  Line *bottom_wall;
  Line *middle_net;

  Paddle *player1;
  Paddle *player2;

  Ball *ball;

  // fix this to check the bounding box
  bool is_out_of_width(int w) { return w > width || w < 0; }

  // fix this to check the bounding box
  bool is_out_of_height(int h) { return h > height || h < 0; }

  // fix this to check the bounding box
  bool is_out_of_bounds(position2D pos) {
    return is_out_of_height(pos.y) || is_out_of_width(pos.x);
  }

  // fix this to check the bounding box
  bool is_ball_touching_paddle(position2D new_ball_pos, Paddle &paddle) {
    int collision_distance_x = (ball->dim._1 / 2 + paddle.dim._2 / 2);
    int collision_distance_y = (ball->dim._1 / 2 + paddle.dim._1 / 2);

    bool is_paddle_p1 = paddle.position.x < width / 2;
    bool is_approach_top = paddle.position.y >= new_ball_pos.y;

    if ((paddle.position.y - new_ball_pos.y) * (is_approach_top ? 1 : -1) >
        collision_distance_y) {
      return false;
    }

    return (paddle.position.x - new_ball_pos.x) * (is_paddle_p1 ? -1 : 1) <=
           collision_distance_x;
  }

  // decouple height as a reference for pixels
  position2D move_entity(position2D prevPos, motion2D &motion) {
    position2D pos = {
        prevPos.x + int(motion.velocity * std::cos(motion.angle) * height),
        prevPos.y + int(motion.velocity * std::sin(motion.angle) * height),
    };
    return pos;
  }

  double bounce_entity(double angle, bool is_vertical_collision) {
    return 2 * PI + (2 * (is_vertical_collision ? 0 : PI / 2)) - angle;
  }

  void handle_ball_collision() {
    // printf("before :: (%d %d), (%f %f)\n", ball->position.x,
    // ball->position.y,
    //        ball->motion.angle, ball->motion.velocity);
    auto new_pos = move_entity(ball->position, ball->motion);
    // printf("before :: [%d] (%d %d)\n", ball->dim._1, new_pos.x, new_pos.y);

    // printf("before :: (%d, %d, %d)\n",
    // is_out_of_height(new_pos.y + ball->dim._1),
    // is_out_of_width(new_pos.y + ball->dim._1),
    // is_ball_touching_paddle(*ball, *player1) ||
    //     is_ball_touching_paddle(*ball, *player2));

    if (is_out_of_height(new_pos.y + (ball->dim._1 / 2)) ||
        is_out_of_height(new_pos.y - (ball->dim._1 / 2))) {
      ball->motion.angle = bounce_entity(ball->motion.angle, true);
      return;
    }
    if (is_out_of_width(new_pos.x + (ball->dim._1 / 2)) -
        is_out_of_width(new_pos.x - (ball->dim._1 / 2))) {
      ball->motion.angle = bounce_entity(ball->motion.angle, false);
      return;
    }
    if (is_ball_touching_paddle(new_pos, *player1) ||
        is_ball_touching_paddle(new_pos, *player2)) {
      ball->motion.angle = bounce_entity(ball->motion.angle, false);
    }

    ball->position.x = new_pos.x;
    ball->position.y = new_pos.y;
  }

  void handle_paddle_collision(Paddle &paddle) {

    auto new_pos = move_entity(paddle.position, paddle.motion);

    if (is_out_of_height(new_pos.y + (paddle.dim._1 / 2)) ||
        is_out_of_height(new_pos.y - (paddle.dim._1 / 2))) {
      return;
    }
    paddle.position.x = new_pos.x;
    paddle.position.y = new_pos.y;
  }

  void draw_frame(Frame *frame) {
    frame->wipe();
    frame->draw_rect(player1->dim, player1->position);
    frame->draw_rect(player2->dim, player2->position);
    frame->draw_rect(ball->dim, ball->position);
    frame->draw_line(middle_net->dim, middle_net->start, middle_net->end);
    frame->draw_line(top_wall->dim, top_wall->start, top_wall->end);
    frame->draw_line(bottom_wall->dim, bottom_wall->start, bottom_wall->end);
    frame->draw_line(right_wall->dim, right_wall->start, right_wall->end);
    frame->draw_line(left_wall->dim, left_wall->start, left_wall->end);
    frame->render();
  }

  void update_entity_positions(Frame *frame) {
    handle_paddle_collision(*player1);
    handle_paddle_collision(*player2);
    handle_ball_collision();

    draw_frame(frame);
    // do random stuff
  }
};

std::mt19937 get_rand_gen();
Field *init_field(int width, int height, std::mt19937 &gen);
void start_game(int canvas_width, int canvas_height);
void start_game_loop(Frame *frame, Field *field);
void render_frame(Frame *frame);

class GlobalField {
public:
  std::mt19937 random_gen;
  Field *field = nullptr;

  static GlobalField &getInstance() {
    static GlobalField ref;
    return ref;
  }

  void init(int width, int height) {
    if (field == nullptr) {
      random_gen = get_rand_gen();
      field = init_field(width, height, random_gen);
    }
  }

  Field *getField() { return field; }
};

extern "C" {
EMSCRIPTEN_KEEPALIVE
void init(int canvas_width, int canvas_height) {
  start_game(canvas_width, canvas_height);
}

EMSCRIPTEN_KEEPALIVE
void player1_key_event(bool isUp, bool isDown) {
  auto globalField = GlobalField().getInstance();
  auto field = globalField.getField();

  if (isUp == isDown) {
    field->player1->motion.velocity = 0;
    return;
  }

  if (isDown) {
    field->player1->motion.angle = PI / 2;
    field->player1->motion.velocity = PADDLE_VELOCITY;
    return;
  }
  if (isUp) {
    field->player1->motion.angle = -PI / 2;
    field->player1->motion.velocity = PADDLE_VELOCITY;
    return;
  }
}

EMSCRIPTEN_KEEPALIVE
void player2_key_event(bool isUp, bool isDown) {
  printf("got event p2 %d %d \n", isUp, isDown);
  auto globalField = GlobalField().getInstance();
  auto field = globalField.getField();

  if (isUp == isDown) {
    field->player2->motion.velocity = 0;
    return;
  }

  if (isDown) {
    field->player2->motion.angle = PI / 2;
    field->player2->motion.velocity = PADDLE_VELOCITY;
    return;
  }
  if (isUp) {
    field->player2->motion.angle = -PI / 2;
    field->player2->motion.velocity = PADDLE_VELOCITY;
    return;
  }
}
};

std::mt19937 get_rand_gen() {
  std::random_device rd;
  std::mt19937 gen(rd());

  return gen;
}

Field *init_field(int width, int height, std::mt19937 &gen) {
  std::uniform_real_distribution<double> rand_distr(0.05, 0.4);

  Field *field = new Field();
  field->width = width;
  field->height = height;

  Line *left_wall = new Line();
  left_wall->start = {0, 0};
  left_wall->end = {0, height - 1};
  left_wall->hasCollision = true;
  field->left_wall = left_wall;

  Line *top_wall = new Line();
  top_wall->start = {0, 0};
  top_wall->end = {width - 1, 0};
  top_wall->hasCollision = true;
  field->top_wall = top_wall;

  Line *right_wall = new Line();
  right_wall->start = {width - 1, 0};
  right_wall->end = {width - 1, height - 1};
  right_wall->hasCollision = true;
  field->right_wall = right_wall;

  Line *bottom_wall = new Line();
  bottom_wall->start = {0, height - 1};
  bottom_wall->end = {width - 1, height - 1};
  bottom_wall->hasCollision = true;
  field->bottom_wall = bottom_wall;

  Line *middle_net = new Line();
  middle_net->start = {width / 2, 0};
  middle_net->end = {width / 2, height - 1};
  middle_net->dim._1 = height;
  middle_net->dim.shape = LINE;
  middle_net->hasCollision = true;
  field->middle_net = middle_net;

  Paddle *player1 = new Paddle();
  player1->dim._1 = height * PADDLE_HEIGHT_PERC;
  player1->dim._2 = width * PADDLE_WIDTH_PERC;
  player1->dim.shape = RECT;
  player1->position = {int(width * PADDLE_WIDTH_OFFSET_PERC), height / 2};
  player1->hasCollision = true;
  field->player1 = player1;

  Paddle *player2 = new Paddle();
  player2->dim._1 = height * PADDLE_HEIGHT_PERC;
  player2->dim._2 = width * PADDLE_WIDTH_PERC;
  player2->dim.shape = RECT;
  player2->position = {int(width * (1 - PADDLE_WIDTH_OFFSET_PERC)), height / 2};
  player2->hasCollision = true;
  field->player2 = player2;

  Ball *ball = new Ball();
  ball->motion = {0.1 * MAX_BALL_VELOCITY, rand_distr(gen) * 2 * PI + (PI / 2)};
  ball->position = {width / 2, height / 2};
  ball->dim._1 = height * BALL_SIZE_PERC;
  ball->dim._2 = height * BALL_SIZE_PERC;
  ball->dim.shape = RECT;
  ball->hasCollision = true;
  field->ball = ball;

  return field;
}

void start_game_loop(Frame *frame, Field *field) {
  bool is_init = false;
  while (1) {
    auto start = chrono::steady_clock::now();

    if (!is_init) {
      field->draw_frame(frame);
      is_init = true;
      continue;
    }

    field->update_entity_positions(frame);

    auto after = chrono::steady_clock::now();
    auto diff = chrono::duration_cast<chrono::milliseconds>(after - start);

    int ms = static_cast<int>((FRAME_BUDGET * 1000) - diff.count());
    if (ms > 0) {
      emscripten_sleep(ms);
      // this_thread::sleep_for(chrono::milliseconds(ms));
    }
  }
}

void start_game(int canvas_width, int canvas_height) {
  GlobalField &globalField = GlobalField::getInstance();
  globalField.init(canvas_width, canvas_height);
  Field *field = globalField.getField();

  Frame *frame = new Frame();
  frame->construct(canvas_width, canvas_height);

  printf("created frame: %d x %d\n", canvas_width, canvas_height);
  start_game_loop(frame, field);
  printf("end flush\n");
}
