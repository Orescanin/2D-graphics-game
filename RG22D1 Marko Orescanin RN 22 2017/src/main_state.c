
#include <main_state.h>
#include <glad/glad.h>
#include <math.h>


#include <rafgl.h>

#include <game_constants.h>

static rafgl_raster_t doge;
static rafgl_raster_t upscaled_doge;
static rafgl_raster_t raster;
static rafgl_raster_t checker;
static rafgl_pixel_rgb_t sampled;
static rafgl_pixel_rgb_t result, rgb_start[1280][720], rgb_gray[1280][720];

static rafgl_texture_t texture;

static rafgl_spritesheet_t hero;
static rafgl_spritesheet_t eye;

//17 razlicitih tajlova i 17 rastera koji ce da predstavljaju te tajlove i koje cemo da ucitamo
#define NUMBER_OF_TILES 17
rafgl_raster_t tiles[NUMBER_OF_TILES];

//velicina matrice, koliki da nam bude svet i ta matrica
#define WORLD_SIZE 128
int tile_world[WORLD_SIZE][WORLD_SIZE];
int tile_eye[WORLD_SIZE][WORLD_SIZE];

#define TILE_SIZE 64

static int raster_width = RASTER_WIDTH, raster_height = RASTER_HEIGHT;

//za skrinsotovanje
static char save_file[256];
int save_file_no = 0;

//kamera x i kamera y je postavljeno na 600,600 na pocetku


int selected_x, selected_y;

//hero stvari:
int pressed;

int animation_running = 0;
int animation_frame = 0;
int direction = 0;
int hero_pos_x = 1280 / 2 - 30;
int hero_pos_y = 720 / 2 - 30;
int cx = 1280 / 2;
int cy = 720 / 2;
int cr = 600;
float dist=0;
int hero_speed = 4;
int hover_frames = 0;
int camx = 3840, camy = 3840;
int flag = 0, eye_sheet = 0;
int brightness_tmp[1280][720];
int collected = 0;

#define MAX_PARTICLES 500
typedef struct _particle_t
{
    float x, y, dx, dy;
    int life;


} particle_t;

particle_t particles[MAX_PARTICLES];

void init_tilemap(void)
{
    int x, y;

    //prolazimo kroz svet i randomujemo
    //i imamo 20% ssanse za generisanje drveca i 80%sanse da generisemo travu
    //
    for(y = 0; y < WORLD_SIZE; y++)
    {
        for(x = 0; x < WORLD_SIZE; x++)
        {
            //obezbedjujemo da ne moze da se spawnuje drvece u centru, heroj udara u drvece
            if(randf() <0.25f && (x!=(camx + hero_pos_x)/64 || y!=(camy + hero_pos_y)/64)
                             && (x!=(camx + hero_pos_x)/64 +1 || y!=(camy + hero_pos_y)/64)
                             &&(x!=(camx + hero_pos_x)/64 || y!=(camy + hero_pos_y)/64+1)
                             &&(x!=(camx + hero_pos_x)/64 + 1 || y!=(camy + hero_pos_y)/64+1))
            {
                //generisanje drveca, tajlovi sa indeksima 3,4,5
                tile_world[y][x] = 3 + rand() % 3;
                //drvece[y][x]= tile_world[y][x];
            }
            else
            {
                //generisanje trave, tajlovi sa indeksima 0,1,2
                //ako je trava ima sanse da se spawnuje oko sa jednim od 4 frame-a tako da
                //bi svaki kretao od druge animacije
                tile_world[y][x] = rand() % 3;
                    tile_eye[y][x] = 0;
                if(randf() < 0.05f)
                    tile_eye[y][x] = rand() % 4 +1;
                //drvece[y][x] = 0;
            }
        }
    }
}



void render_tilemap(rafgl_raster_t *raster)
{
    int x, y;
    int x0 = camx / TILE_SIZE;

    int x1 = x0 + (raster_width / TILE_SIZE) + 1;
    int y0 = camy / TILE_SIZE;
    int y1 = y0 + (raster_height / TILE_SIZE) + 2;

    //klempujemo
    if(x0 < 0) x0 = 0;
    if(y0 < 0) y0 = 0;
    if(x1 < 0) x1 = 0;
    if(y1 < 0) y1 = 0;

    //klempujemo
    if(x0 >= WORLD_SIZE) x0 = WORLD_SIZE - 1;
    if(y0 >= WORLD_SIZE) y0 = WORLD_SIZE - 1;
    if(x1 >= WORLD_SIZE) x1 = WORLD_SIZE - 1;
    if(y1 >= WORLD_SIZE) y1 = WORLD_SIZE - 1;

    //pointer ka tipu tajla kojeg trenutno iscrtavamo
    //inace tajlovi su nam ucitani dole u for petlji u inicijalizaciji
    rafgl_raster_t *draw_tile;


    //
    for(y = y0; y <= y1; y++)
    {
        for(x = x0; x <= x1; x++)
        {

                draw_tile = tiles + (tile_world[y][x] % NUMBER_OF_TILES);
                rafgl_raster_draw_raster(raster, draw_tile, x * TILE_SIZE - camx, y * TILE_SIZE - camy - draw_tile->height + TILE_SIZE);
                if(tile_eye[y][x] > 0)
                   // spawnujemo oci na njihovim tajlovima gde samo vrtimo 4 frame-a prvog reda, delimo sa 7 da
                   //bi usporili animaciju i povecavamo brojac eyesheet da bi cirkulisale
                    rafgl_raster_draw_spritesheet(raster, &eye, (tile_eye[y][x]+eye_sheet/7)%4, 0, x * TILE_SIZE - camx, y * TILE_SIZE - camy - draw_tile->height + TILE_SIZE);

        }
    }
     eye_sheet++;

    rafgl_raster_draw_rectangle(raster, selected_x * TILE_SIZE - camx, selected_y * TILE_SIZE - camy, TILE_SIZE, TILE_SIZE, rafgl_RGB(255, 255, 0));



}

void draw_particles(rafgl_raster_t *raster)
{
    int i;
    particle_t p;
    for(i = 0; i < MAX_PARTICLES; i++)
    {
        p = particles[i];
        if(p.life <= 0) continue;
        //funkcijom draw line iscrtavamo liniju (uradjeno preko bresenhamnov alg. za iscrtavanje duzi)
        //
        //kako radi: na nekom rasteru od tacke x0, y0, do x1, y1, isrtaj duz u boji toj i toj
        //x0 i y0 se racunaju tako sto od trenutne pozicije oduzmemo brzinu kojom se krecemo, dakle to nam je otprilike prethodna pozicija
        rafgl_raster_draw_line(raster, p.x - p.dx, p.y - p.dy, p.x, p.y, rafgl_RGB(rand()%256, rand()%256, rand()%256));
    }
}

static float elasticity = 0.6;

void update_particles(float delta_time)
{
    int i;
    for(i = 0; i < MAX_PARTICLES; i++)
    {
        if(particles[i].life <= 0) continue;

        particles[i].life--;

        particles[i].x += particles[i].dx;
        particles[i].y += particles[i].dy;
        particles[i].dx *= 0.995f;
        particles[i].dy *= 0.995f;
        particles[i].dy += 0.05;


        if(particles[i].x < 0)
        {
            particles[i].x = 0;
            //elasticity nam je gubitak u svakom slucaju, ali imamo i neki random gubitak
            //da se ne bi svi partikli odbijali istom jacinom od zida
            particles[i].dx = (rafgl_abs_m(particles[i].dx)) * randf() * elasticity;
        }

        //analogno za y
        //odnosno "udaranje o plafon"
        if(particles[i].y < 0)
        {
            particles[i].y = 0;
            particles[i].dy = (rafgl_abs_m(particles[i].dy)) * randf() * elasticity;
        }

        //udaranje u desni zid
        //minus elasticity zbog obrtanja smera
        if(particles[i].x >= raster_width)
        {
            particles[i].x = raster_width - 1;
            particles[i].dx = (rafgl_abs_m(particles[i].dx)) * randf() * (-elasticity);
        }

        //analogno za "udaranje u pod"
        if(particles[i].y >= raster_height)
        {
            particles[i].y = raster_height - 1;
            particles[i].dy = (rafgl_abs_m(particles[i].dy)) * randf() * (-elasticity);
        }

    }
}


void main_state_init(GLFWwindow *window, void *args, int width, int height)
{
    /* inicijalizacija */
    /* raster init nam nije potreban ako radimo load from image */
    rafgl_raster_load_from_image(&doge, "res/images/kewt.jpg");
    rafgl_raster_load_from_image(&checker, "res/images/checker32.png");

    raster_width = width;
    raster_height = height;

    rafgl_raster_init(&upscaled_doge, raster_width, raster_height);
    rafgl_raster_bilinear_upsample(&upscaled_doge, &doge);


    rafgl_raster_init(&raster, raster_width, raster_height);
//    rafgl_raster_init(&raster2, raster_width, raster_height);
    int y,x;
    float xn, yn;




    int i;
    rafgl_window_set_title("IGRICA NAJBOLJA");

    //naziv putanje do tajla kog zelimo da ucitamo
    char tile_path[256];

    //prolazimo kroz sve tajlove i ucitavamo slike u rastere za odgovarajuce pozicije u nizu
    for(i = 0; i < NUMBER_OF_TILES; i++)
    {
        //ovde ce see samo zameniti ovo %d sa trenutnim indeksom u foru
        //tako da cemo u tiles niz imati ucitane sve slike redosledom od 0 do16
        sprintf(tile_path, "res/tiles/svgset%d.png", i);
        rafgl_raster_load_from_image(&tiles[i], tile_path);
    }

    //i sada kada imamo ucitane tajloce mozemo i da inicijalizujemo tilemapu
    init_tilemap();
    rafgl_spritesheet_init(&hero, "res/images/character.png", 10, 4);
    rafgl_spritesheet_init(&eye, "res/images/eyeball.png", 5, 5);

    rafgl_texture_init(&texture);


}


int pressed;
float location = 0;
float selector = 0;


void main_state_update(GLFWwindow *window, float delta_time, rafgl_game_data_t *game_data, void *args)
{
    /* hendluj input */

    int i, gen = 50, radius = 10;
    float angle, speed;

    if(game_data->is_lmb_down && game_data->is_rmb_down)
    {
        pressed = 1;
        location = rafgl_clampf(game_data->mouse_pos_y, 0, raster_height - 1);
        selector = 1.0f * location / raster_height;
    }
    else
    {
        pressed = 0;
    }


    //HERO STVARI:

    //pretpostavimo da se krecemo
    animation_running = 1;




    if(game_data->keys_down[RAFGL_KEY_W])
    {

        //hero_pos_y = hero_pos_y - hero_speed * delta_time;
        //if(tile_world[(662+camx)/64][(camy+414-hero_speed)/64]<3)

        //proveravamo da li pozicija heroja, to jest centra ekrana nije na tile-u koje je drvo
        //ako nije pomeramo kameru ako jeste nista
        if(tile_world[(camy+hero_pos_y-hero_speed+TILE_SIZE/2)/64][(hero_pos_x+camx+TILE_SIZE/2)/64]<3)
            camy = camy - hero_speed;

        //proveravamo da li je pozicija heroja, to jest centra ekrana na tileu koji je oko
        //ako jeste, unistavamo oko, stavljamo 0 u njegovu tile mapu, povecavamo krug vidljivosti
        // i brightness, i tako u krug za svaki pravac

        if(tile_eye[(camy+hero_pos_y-hero_speed+TILE_SIZE/2)/64][(hero_pos_x+camx+TILE_SIZE/2)/64]>0){
            tile_eye[(camy+hero_pos_y-hero_speed+TILE_SIZE/2)/64][(hero_pos_x+camx+TILE_SIZE/2)/64]=0;
            cr+=100;
            collected++;
            //Nista samo pravimo partikle kada unisti oko isto kao na casu nista specijalno, samo
            //im je kraci zivotni vek i sareni su
             for(i = 0; (i < MAX_PARTICLES) && gen; i++)
            {


                if(particles[i].life <= 0)
                {
                    particles[i].life = 20 * randf() + 20;
                    particles[i].x = hero_pos_x;
                    particles[i].y = hero_pos_y;


                    angle = randf() * M_PI *  2.0f;
                    speed = ( 0.3f + 0.7 * randf()) * radius;
                    particles[i].dx = cosf(angle) * speed;
                    particles[i].dy = sinf(angle) * speed;
                    gen--;

                }
            }

        }

        direction = 2;
        cr--;
    }

    else if(game_data->keys_down[RAFGL_KEY_S])
    {
        //if(tile_world[camx/64][(camy+hero_speed)/64]<3)
        if(tile_world[(camy+hero_pos_y+hero_speed+TILE_SIZE)/64][(hero_pos_x+camx+TILE_SIZE/2)/64]<3)
        //hero_pos_y = hero_pos_y + hero_speed * delta_time;
            camy = camy+ hero_speed;//* delta_time;
        if(tile_eye[(camy+hero_pos_y+hero_speed+TILE_SIZE)/64][(hero_pos_x+camx+TILE_SIZE/2)/64]>0){
            tile_eye[(camy+hero_pos_y+hero_speed+TILE_SIZE)/64][(hero_pos_x+camx+TILE_SIZE/2)/64] = 0;
            cr+=100;
            collected++;

            for(i = 0; (i < MAX_PARTICLES) && gen; i++)
            {


                if(particles[i].life <= 0)
                {
                    particles[i].life = 20 * randf() + 20;
                    particles[i].x = hero_pos_x;
                    particles[i].y = hero_pos_y;


                    angle = randf() * M_PI *  2.0f;
                    speed = ( 0.3f + 0.7 * randf()) * radius;
                    particles[i].dx = cosf(angle) * speed;
                    particles[i].dy = sinf(angle) * speed;
                    gen--;

                }
            }
        }
        direction = 0;
        cr--;
    }

    else if(game_data->keys_down[RAFGL_KEY_A])
    {
        //if(tile_world[(camx-hero_speed)/64+11][camy/64+7]<3)
//hero_pos_x = hero_pos_x - hero_speed * delta_time;

        if(tile_world[(camy+hero_pos_y+TILE_SIZE)/64][(hero_pos_x+camx-hero_speed)/64]<3){
                    camx =  camx- hero_speed;//* delta_time;
        if(tile_eye[(camy+hero_pos_y+TILE_SIZE)/64][(hero_pos_x+camx-hero_speed)/64]>0){
            tile_eye[(camy+hero_pos_y+TILE_SIZE)/64][(hero_pos_x+camx-hero_speed)/64] = 0;
            cr+=100;
            collected++;
            for(i = 0; (i < MAX_PARTICLES) && gen; i++)
            {


                if(particles[i].life <= 0)
                {
                    particles[i].life = 20 * randf() + 20;
                    particles[i].x = hero_pos_x;
                    particles[i].y = hero_pos_y;


                    angle = randf() * M_PI *  2.0f;
                    speed = ( 0.3f + 0.7 * randf()) * radius;
                    particles[i].dx = cosf(angle) * speed;
                    particles[i].dy = sinf(angle) * speed;
                    gen--;

                }
            }
        }
        cr--;
    }
        direction = 1;

    }

    else if(game_data->keys_down[RAFGL_KEY_D])
    {
        //hero_pos_x = hero_pos_x + hero_speed * delta_time;
        //if(tile_world[(camx+hero_speed)/64+11][camy/64+7]<3)
        if(tile_world[(camy+hero_pos_y+TILE_SIZE)/64][(hero_pos_x+camx+hero_speed+TILE_SIZE)/64]<3)
            camx = camx+ hero_speed;//* delta_time;
        if(tile_eye[(camy+hero_pos_y+TILE_SIZE)/64][(hero_pos_x+camx+hero_speed+TILE_SIZE)/64]>0){
            tile_eye[(camy+hero_pos_y+TILE_SIZE)/64][(hero_pos_x+camx+hero_speed+TILE_SIZE)/64] = 0;
            cr+=100;
            collected++;
            for(i = 0; (i < MAX_PARTICLES) && gen; i++)
            {


                if(particles[i].life <= 0)
                {
                    particles[i].life = 20 * randf() + 20;
                    particles[i].x = hero_pos_x;
                    particles[i].y = hero_pos_y;


                    angle = randf() * M_PI *  2.0f;
                    speed = ( 0.3f + 0.7 * randf()) * radius;
                    particles[i].dx = cosf(angle) * speed;
                    particles[i].dy = sinf(angle) * speed;
                    gen--;

                }
            }
        }
        direction = 3;
       cr--;
    }
    else
    {
        //ipak se ne krecemo
        animation_running = 0;
    }

    //ako zelimo da animiramo, tj kliknuto je nesto od wasd, dakle krecemo se, pa zato i hocemo da promenimo polozaj
    if(animation_running)
    {
        //hover frames je koliko frameova zelimo da stojimo na jednom spriteu, tj svaki sprite ce se zadrzati po 5 frameova
        //i tek kad hover padne na 0 mi cemo preci na sledecu animaciju
        if(hover_frames == 0)
        {
            //i samo kazemo ovo, tj kazemo da se kroz ove animacije krecemo na desno
            //i ako smo dosli do kraja, da se vratimo na pocetku tog reda
            //tj animation frame nam je x pozicija u jednom redu dokle smo stigli, kao x iz spritesheeta
            animation_frame = (animation_frame + 1) % 10;
            //kada bismo ovde stavili 1, menjali bismo animaciju kretanja u svakom frameu, pa bi nam bilo prebrzo
            hover_frames = 5;
        }
        else
        {
            //smanjujemo hover dok ne dodjemo do 0
            hover_frames--;
        }

    }


//KRAJ HERO STVARI


    //racunanje u kom polju mape se nalazi mis
    //racunanje je objasnjeno na pocetku koda
    //klempovano izmedju 0 i world_sizea
    //kada ne bismo imali ovo klempovanje, samo bismo mogli da selektujemo i van worlda
    selected_x = rafgl_clampi((game_data->mouse_pos_x + camx) / TILE_SIZE, 0, WORLD_SIZE - 1);
    selected_y = rafgl_clampi((game_data->mouse_pos_y + camy) / TILE_SIZE, 0, WORLD_SIZE - 1);

    /* izmeni raster */

    int x, y;

    float xn, yn;

    rafgl_pixel_rgb_t sampled, sampled2, resulting, resulting2;




    //kada se pritisne plus da se samo menja tajl koji je selektovan
    //menjamo brojeve u matrici tile_world od kojih nam zavisi koji tajl cemo da setujemo na tu poziciju
    if(game_data->keys_pressed[RAFGL_KEY_KP_ADD])
    {
        tile_world[selected_y][selected_x]++;
        tile_world[selected_y][selected_x] %= NUMBER_OF_TILES;
    }

    update_particles(delta_time);
    //na kraju samo pozovemo i renderovanje tog rastera
    render_tilemap(&raster);
    for(y = 0; y < raster_height; y++)
    {
        yn = 1.0f * y / raster_height;
        for(x = 0; x < raster_width; x++)
        {
            dist = rafgl_distance2D(cx,cy,x,y);
            // za krug je prosto ako smo u krugu crtaj normalno ako ne sve crno
            if(dist >= cr){
//
                pixel_at_m(raster, x, y).rgba = rafgl_RGB(0, 0, 0);
            }
            else{


                    sampled = pixel_at_m(raster,x,y);
                   int brightness = (sampled.r + sampled.g + sampled.b)/3;


                    //svako boju vracamo na staro sakupljanjem ocjiu, tako sto samo dodajemo na
                    //brightness dok ne dobijemo staru boju, kolikim brojem delimo collected(sakuplene oci)
                    //toliko nam njih treba do stare boje
                    result.r = brightness + (sampled.r - brightness)*collected/10;
                    result.g = brightness + (sampled.g - brightness)*collected/10;
                    result.b = brightness + (sampled.b - brightness)*collected/10;
                    //dodelimo da je nas pixel result
                    pixel_at_m(raster,x,y) = result;



            }
        }
    }
    flag=1;
     rafgl_raster_draw_spritesheet(&raster, &hero, animation_frame, direction, hero_pos_x, hero_pos_y);

     draw_particles(&raster);

    /* shift + s snima raster */
    //za skrinsotovanje ekrana
    if(game_data->keys_pressed[RAFGL_KEY_S] && game_data->keys_down[RAFGL_KEY_LEFT_SHIFT])
    {
        sprintf(save_file, "save%d.png", save_file_no++);
        rafgl_raster_save_to_png(&raster, save_file);
    }

}


void main_state_render(GLFWwindow *window, void *args)
{
    /* prikazi teksturu */
    rafgl_texture_load_from_raster(&texture, &raster);
    rafgl_texture_show(&texture, 0);
}


void main_state_cleanup(GLFWwindow *window, void *args)
{
    rafgl_raster_cleanup(&raster);
   // rafgl_raster_cleanup(&raster2);
    rafgl_texture_cleanup(&texture);

}
